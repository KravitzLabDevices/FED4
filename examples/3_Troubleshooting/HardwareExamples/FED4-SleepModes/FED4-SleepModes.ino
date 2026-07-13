/*
 * FED4 Sleep Modes Test
 *
 * Alternates 5 s light sleep (display maintained, VCOM keepalive, touch wake)
 * and 5 s deep sleep (panel blanked, PSV3 off, PSV2 off, sensors in lowest power).
 * Before PSV2 off: quiesce SDIO, photogate GPIO, and I2S amp lines to limit
 * back-power into the unpowered 3.3V2 domain. EXP_AMP_SD held LOW → MAX98357A
 * shutdown (~0.6 µA), not 2.4 mA active Iq.
 *
 * Standalone — no FED4.h. See FED1-7-0_TESTLOG.md for v1.7 power rail map.
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display pins).
 * Designed for battery: setup uses a fixed delay only — never blocks on Serial.
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include "SparkFun_VL53L1X.h"
#include <Fonts/FreeSans9pt7b.h>
#include <FED4_Pins.h>

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
  {                         \
    int16_t t = a;          \
    a = b;                  \
    b = t;                  \
  }
#endif

static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;
static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;
static const uint32_t SPI_HZ = 1000000;

static const uint32_t SLEEP_MS = 5000;
static const uint32_t VCOM_MS = 500;
static const float TOUCH_THRESHOLD = 0.20f;

static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t sleepCycle = 0;

Adafruit_MCP23X17 mcp;
Adafruit_LIS3DH accel;
Adafruit_BME680 bme;
Adafruit_VEML7700 veml;
SFEVL53L1X tofSensor(Wire);

bool accelOk = false;
bool bmeOk = false;
bool vemlOk = false;
bool tofOk = false;

uint16_t baseLeft = 0;
uint16_t baseCenter = 0;
uint16_t baseRight = 0;

// ---------------------------------------------------------------------------
// MIP display (minimal, from FED4-Display-Standalone)
// ---------------------------------------------------------------------------

class MIPDisplay : public Adafruit_GFX {
public:
  MIPDisplay() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

  bool begin() {
    const uint32_t bufferSize = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8;
    if (frameBuffer) {
      free(frameBuffer);
      frameBuffer = nullptr;
    }
    frameBuffer = (uint8_t *)malloc(bufferSize);
    if (!frameBuffer) return false;
    memset(frameBuffer, 0x00, bufferSize);
    setRotation(1);
    return true;
  }

  void clearBlack() {
    memset(frameBuffer, 0x00, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void refresh() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
    delay(4);

    delayMicroseconds(30);
    digitalWrite(DISPLAY_CS, HIGH);
    delay(5);

    SPI.beginTransaction(SPISettings(SPI_HZ, LSBFIRST, SPI_MODE0));

    const uint8_t bytesPerLine = PANEL_WIDTH / 8;
    for (uint8_t line = 0; line < PANEL_HEIGHT; line++) {
      SPI.transfer(line);
      const uint8_t *row = frameBuffer + (uint32_t)line * bytesPerLine;
      for (uint8_t b = 0; b < bytesPerLine; b++) {
        SPI.transfer(row[b]);
      }
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
    }

    SPI.endTransaction();
    delay(2);
    digitalWrite(DISPLAY_CS, LOW);
  }

  void toggleVcomKeepAlive() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
  }

  void setVcomLow() {
    vcomState = false;
    digitalWrite(DISPLAY_VCOM, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

    int16_t px = x;
    int16_t py = y;

    switch (rotation) {
      case 1:
        _swap_int16_t(px, py);
        px = PANEL_WIDTH - 1 - px;
        break;
      case 2:
        px = PANEL_WIDTH - 1 - px;
        py = PANEL_HEIGHT - 1 - py;
        break;
      case 3:
        _swap_int16_t(px, py);
        py = PANEL_HEIGHT - 1 - py;
        break;
      default:
        break;
    }

    if (color)
      frameBuffer[(py * PANEL_WIDTH + px) / 8] |= pgm_read_byte(&set[px & 7]);
    else
      frameBuffer[(py * PANEL_WIDTH + px) / 8] &= pgm_read_byte(&clr[px & 7]);
  }

private:
  uint8_t *frameBuffer = nullptr;
  bool vcomState = false;
};

MIPDisplay display;

// ---------------------------------------------------------------------------
// Power rails (TPS22917 active-low: LOW = ON, HIGH = OFF)
// ---------------------------------------------------------------------------

void psv2On() {
  mcp.digitalWrite(EXP_PSV2_EN, LOW);
  delayMicroseconds(100);
}

void psv2Off() {
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
}

void psv3On() {
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delayMicroseconds(100);
}

void psv3Off() {
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void displayReset() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  pinMode(DISPLAY_VCOM, OUTPUT);
  display.setVcomLow();

  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

void displayEnsureOn() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
}

void displayBlank() {
  pinMode(DISPLAY_VCOM, OUTPUT);
  display.setVcomLow();
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
}

void drawStatus(const char *modeLabel) {
  display.clearBlack();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(8, 36);
  display.print("FED4 SleepModes");

  display.setFont(nullptr);
  display.setTextSize(2);
  display.setCursor(8, 80);
  display.print(modeLabel);

  display.setTextSize(1);
  display.setCursor(8, 120);
  display.print("Cycle: ");
  display.print(sleepCycle);

  display.setCursor(8, 140);
  display.print("Boot: ");
  display.print(bootCount);

  display.setCursor(8, 300);
  display.print("BL off | 5s each");

  display.refresh();
}

// ---------------------------------------------------------------------------
// Wake / touch helpers
// ---------------------------------------------------------------------------

void printWakeReason() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  switch (reason) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wake: timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wake: touchpad");
      break;
    case ESP_SLEEP_WAKEUP_GPIO:
      Serial.println("Wake: gpio");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Serial.println("Wake: cold boot");
      break;
    default:
      Serial.printf("Wake: other (%d)\n", reason);
      break;
  }
}

void calibrateTouch() {
  delay(30);
  baseLeft = touchRead(TOUCH_PAD_LEFT);
  baseCenter = touchRead(TOUCH_PAD_CENTER);
  baseRight = touchRead(TOUCH_PAD_RIGHT);
  Serial.printf("Touch baseline L:%u C:%u R:%u\n", baseLeft, baseCenter, baseRight);
}

bool touched(uint16_t current, uint16_t baseline) {
  if (baseline == 0) return false;
  float dev = fabs((float)current / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void printLikelyTouchWake() {
  uint16_t l = touchRead(TOUCH_PAD_LEFT);
  uint16_t c = touchRead(TOUCH_PAD_CENTER);
  uint16_t r = touchRead(TOUCH_PAD_RIGHT);

  if (touched(l, baseLeft)) Serial.println("Likely touch: LEFT");
  if (touched(c, baseCenter)) Serial.println("Likely touch: CENTER");
  if (touched(r, baseRight)) Serial.println("Likely touch: RIGHT");
}

// ---------------------------------------------------------------------------
// Peripheral init / shutdown (always-on 3.3V I2C sensors)
// ---------------------------------------------------------------------------

bool initSensors() {
  accelOk = accel.begin(I2C_ADDR_ACCEL);
  if (accelOk) {
    accel.setRange(LIS3DH_RANGE_2_G);
    accel.setDataRate(LIS3DH_DATARATE_50_HZ);
    accel.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);
    Serial.println("OK: LIS2DH12TR");
  } else {
    Serial.println("WARN: LIS2DH12TR");
  }

  bmeOk = bme.begin(I2C_ADDR_BME680, &Wire);
  if (bmeOk) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("OK: BME680");
  } else {
    Serial.println("WARN: BME680");
  }

  vemlOk = veml.begin(&Wire);
  if (vemlOk) {
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_100MS);
    veml.powerSaveEnable(false);
    veml.enable(true);
    Serial.println("OK: VEML7700");
  } else {
    Serial.println("WARN: VEML7700");
  }

  tofOk = (tofSensor.begin() == 0);
  Serial.println(tofOk ? "OK: VL53L1X" : "WARN: VL53L1X");

  return accelOk || bmeOk || vemlOk || tofOk;
}

void shutdownBme680() {
  if (!bmeOk) return;
  bme.setGasHeater(0, 0);
  // ctrl_meas @ 0x74: sleep mode (mode bits = 00)
  Wire.beginTransmission(I2C_ADDR_BME680);
  Wire.write(0x74);
  Wire.write(0x00);
  Wire.endTransmission();
}

void shutdownSensors() {
  if (accelOk) {
    accel.setPerformanceMode(LIS3DH_MODE_LOW_POWER);
    accel.setDataRate(LIS3DH_DATARATE_POWERDOWN);
  }

  shutdownBme680();

  if (vemlOk) {
    veml.enable(false);
  }

  if (tofOk) {
    tofSensor.stopRanging();
    tofSensor.clearInterrupt();
  }
}

// ---------------------------------------------------------------------------
// GPIO / MCP idle state
// ---------------------------------------------------------------------------

void releaseMotor() {
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);
  digitalWrite(MOTOR_PIN_1, LOW);
  digitalWrite(MOTOR_PIN_2, LOW);
  digitalWrite(MOTOR_PIN_3, LOW);
  digitalWrite(MOTOR_PIN_4, LOW);
}

void prepareOutputsIdle(bool deepSleep) {
  releaseMotor();

  mcp.pinMode(EXP_SOL_1, OUTPUT);
  mcp.pinMode(EXP_SOL_2, OUTPUT);
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_SOL_1, LOW);
  mcp.digitalWrite(EXP_SOL_2, LOW);
  mcp.digitalWrite(EXP_HAPTIC, LOW);
  mcp.digitalWrite(EXP_AMP_SD, LOW);

  displayLight(false);

  pinMode(RGB_STRIP, OUTPUT);
  digitalWrite(RGB_STRIP, LOW);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  pinMode(PIR_MOTION, INPUT_PULLDOWN);

  psv3Off();

  if (deepSleep) {
    psv2Off();
  } else {
    psv2On();
  }
}

// Quiesce SDIO on the shared SPI bus before cutting PSV2 (SD VCC).
void shutdownSdBusForDeepSleep() {
  SPI.end();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  pinMode(SPI_SCK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  digitalWrite(SPI_SCK, LOW);
  digitalWrite(SPI_MOSI, LOW);

  pinMode(SPI_MISO, INPUT_PULLDOWN);

  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
}

// Quiesce ESP32 pins into the PSV2 domain before rail off.
// Photogate front-ends and MAX98357A I2S live on 3.3V2; biased GPIO can
// back-power unpowered circuitry (measured ~+10 mA vs PSV2-on deep sleep).
void shutdownPsv2DomainForDeepSleep() {
  pinMode(PHOTOGATE_1, OUTPUT);
  pinMode(PHOTOGATE_2, OUTPUT);
  pinMode(PHOTOGATE_3, OUTPUT);
  pinMode(PHOTOGATE_4, OUTPUT);
  digitalWrite(PHOTOGATE_1, LOW);
  digitalWrite(PHOTOGATE_2, LOW);
  digitalWrite(PHOTOGATE_3, LOW);
  digitalWrite(PHOTOGATE_4, LOW);

  pinMode(AMP_BCLK, OUTPUT);
  pinMode(AMP_LRCLK, OUTPUT);
  pinMode(AMP_DIN, OUTPUT);
  digitalWrite(AMP_BCLK, LOW);
  digitalWrite(AMP_LRCLK, LOW);
  digitalWrite(AMP_DIN, LOW);
}

// ---------------------------------------------------------------------------
// Sleep entry
// ---------------------------------------------------------------------------

void enterLightSleep5s() {
  Serial.println("Entering light sleep (5 s)...");

  shutdownSensors();
  prepareOutputsIdle(false);
  displayEnsureOn();

  esp_sleep_enable_touchpad_wakeup();

  uint32_t deadline = millis() + SLEEP_MS;
  while ((int32_t)(deadline - millis()) > 0) {
    display.toggleVcomKeepAlive();

    uint32_t remainingMs = deadline - millis();
    uint64_t chunkUs = (uint64_t)min(remainingMs, VCOM_MS) * 1000ULL;
    if (chunkUs < 1000) chunkUs = 1000;

    esp_sleep_enable_timer_wakeup(chunkUs);
    Serial.flush();
    esp_light_sleep_start();

    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    if (reason == ESP_SLEEP_WAKEUP_TOUCHPAD) {
      Serial.println("Light sleep touch wake");
      printLikelyTouchWake();
    }
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
  Serial.println("Light sleep complete.");
}

void enterDeepSleep5s() {
  Serial.println("Entering deep sleep (5 s, PSV2 OFF)...");

  shutdownSensors();
  displayBlank();
  shutdownSdBusForDeepSleep();
  shutdownPsv2DomainForDeepSleep();
  prepareOutputsIdle(true);

  // Touch wake already disabled at end of enterLightSleep5s().
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_MS * 1000ULL);

  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

static const uint32_t SERIAL_BOOT_DELAY_MS = 500;

static void serialStartup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
}

void setup() {
  serialStartup();

  bootCount++;
  Serial.printf("\n=== FED4 SleepModes | Boot %lu ===\n", (unsigned long)bootCount);
  printWakeReason();

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  psv2On();
  psv3On();
  delay(5);

  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.pinMode(EXP_SOL_1, OUTPUT);
  mcp.pinMode(EXP_SOL_2, OUTPUT);
  mcp.pinMode(EXP_HAPTIC, OUTPUT);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  displayReset();
  displayLight(false);

  if (!display.begin()) {
    Serial.println("Display init failed");
    while (1) delay(10);
  }

  releaseMotor();
  initSensors();
  calibrateTouch();

  Serial.println("Ready — alternating light/deep sleep.");
}

void loop() {
  sleepCycle++;
  drawStatus("Light sleep");
  enterLightSleep5s();

  drawStatus("Deep sleep");
  delay(50);

  enterDeepSleep5s();
}
