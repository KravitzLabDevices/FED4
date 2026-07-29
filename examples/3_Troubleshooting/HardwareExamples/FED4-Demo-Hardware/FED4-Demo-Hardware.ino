/*
 * FED4 Demo Hardware
 *
 * Full hardware sweep on Kyocera TN0216 MIP display (standalone, no FED4.h).
 *
 * Architecture (event-driven reference patterns for future FED4 dev):
 *   - Buttons, INT_OR: GPIO interrupts set volatile flags; loop consumes.
 *     B1 backlight is polled with debounce (toggle on release). ISRs never touch I2C.
 *   - Touch: S3 hardware FSM + boot idle baseline. Strip brightness and poke
 *     both use baseline rise from the boot idle average.
 *   - ToF: continuous ranging, polled non-blocking (checkForDataReady).
 *   - BME680: async beginReading()/endReading() — no blocking gas-heater wait.
 *   - Audio: chunked I2S sequencer; while playing the loop skips display/SPI
 *     work and serviceAudio() prefills + feeds ~64 ms per call (3 ms fades).
 *   - Display: redrawn only when displayDirty is set (500 ms telemetry poll
 *     or immediate UI changes).
 *
 * FUTURE DEV NOTES:
 *   - Touch drift: the S3 benchmark auto-tracks slow drift in hardware.
 *     Consider periodically logging touch_pad_read_benchmark() vs the boot
 *     idle average to detect/log drift, and re-baselining on large deviation.
 *   - INT_OR (GPIO 7) ORs device interrupts. Pattern: one CHANGE interrupt on
 *     INT_OR, then query each source (LIS2DH INT, MCP23017 INTA/B, RTC alarm)
 *     to identify and clear it. This demo only tracks the line state.
 *   - Display: MIP panels accept per-line updates. Tracking dirty framebuffer
 *     lines and transmitting only those would cut a full refresh (~45 ms SPI)
 *     to a few lines for small changes.
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display pins).
 * Serial is optional: 1 s grace delay, then runs without a monitor attached.
 *
 * Buttons:
 *   B1 — toggle display backlight (immediate)
 *   B2 — hold for haptic (active while pressed)
 *   B3 — play melody (non-blocking)
 *
 * Pokes: left/right → haptic + tone + dispense (L/R LEDs = touch glow only).
 * Center poke → haptic + intro audio; full-strip rainbow while center pad held.
 * STATUS_LED mirrors PIR; PIR does not trigger audio.
 *
 * Boot: embedded PCM startup clip (sounds/startup_sound.h, ~2.5 s TTS).
 */

// Set to 0 to disable poke tone / dispense (touch strip still active).
#define ENABLE_FEED 1

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Stepper.h>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_MAX1704X.h>
#include <ESP_I2S.h>
#include "SparkFun_VL53L1X.h"
#include "RTClib.h"
#include <Fonts/FreeSans9pt7b.h>
#include <FED4_Pins.h>
#include <FED4_DisplayOrient.h>
#include "driver/touch_sensor.h"
#include "sounds/startup_sound.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

// ---------------------------------------------------------------------------
// Display (Kyocera TN0216)
// ---------------------------------------------------------------------------

static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;
static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;
static const uint32_t SPI_HZ = 1000000;
static const uint32_t POLL_MS = 500;
static const uint32_t STRIP_MS = 8;     // LED fade animation tick
static const uint32_t TOF_CHECK_MS = 25;
static const uint32_t BUTTON_DEBOUNCE_MS = 80;
// ESP32-S3: touch counts RISE when touched (values ~90k-170k idle, uint32_t).
static const float TOUCH_TRIGGER_RISE = 0.05f;   // poke — conservative
static const float TOUCH_LED_FULL_RISE = 0.06f;   // strip full brightness at 6% rise
static const float TOUCH_DEADBAND = 0.015f;      // ignore noise within 1.5% of idle
static const uint8_t TOUCH_LED_MIN_BRIGHT = 28;  // WS2812 floor — below ~10% often invisible
// Motor (28BYJ-48 + ULN2003; match FED4-Motor.ino pin order IN1,IN3,IN2,IN4)
static const uint16_t MOTOR_STEPS = 2048;
static const uint8_t MOTOR_SPEED_RPM = 8;
// S3 touch tuning (see FED4-Touch.ino)
static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
static const uint16_t TOUCH_SLEEP_CYCLES = 500;
static const uint32_t POKE_ARM_MS = 1500; // ignore poke edges until after boot settle
static const uint32_t PELLET_WIPE_MS = 1000; // full strip wipe duration (~1 s)
static const uint32_t ORIENT_MS = 100;  // accel-based display rotation poll

// Called from display SPI refresh so audio/buttons stay serviced mid-transfer.
void cooperativeYield();

static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

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
    setRotation(FED4_DISPLAY_ROTATION_NATIVE);
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
      for (uint8_t b = 0; b < bytesPerLine; b++) SPI.transfer(row[b]);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      if ((line & 7) == 7) cooperativeYield();
    }
    SPI.endTransaction();
    delay(2);
    digitalWrite(DISPLAY_CS, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;
    int16_t px = x, py = y;
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

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

Adafruit_MCP23X17 mcp;
MIPDisplay display;
Adafruit_BME680 bme;
Adafruit_VEML7700 veml;
SFEVL53L1X tofSensor(Wire);
RTC_DS3231 rtc;
Adafruit_MAX17048 maxlipo;
Adafruit_LIS3DH accel = Adafruit_LIS3DH();
I2SClass i2s;

#define NUM_STRIP_LEDS 8
Adafruit_NeoPixel strip(NUM_STRIP_LEDS, RGB_STRIP, NEO_GRB + NEO_KHZ800);
Stepper stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_3, MOTOR_PIN_2, MOTOR_PIN_4);

struct Telemetry {
  bool rtcOk = false, batOk = false, batReady = false, bmeOk = false, luxOk = false;
  bool tofOk = false, accelOk = false;
  uint8_t hour = 0, minute = 0, second = 0;
  float voltage = NAN, percent = NAN;
  float tempC = NAN, humidity = NAN, pressureHpa = NAN, gasKOhm = NAN, lux = NAN;
  int tofMm = -1;
  float accelX = NAN, accelY = NAN, accelZ = NAN;
  uint32_t touchL = 0, touchC = 0, touchR = 0;
  bool pgC = false, pgL = false, pgR = false, pgP = false;
  bool pirHigh = false, intOrLow = false;
  bool dispLedOn = true;
  uint32_t pokeCount = 0, pelletCount = 0;
};

Telemetry telem;
uint32_t touchIdleL = 0, touchIdleC = 0, touchIdleR = 0;
uint32_t touchThreshL = 0, touchThreshC = 0, touchThreshR = 0;
uint8_t stripBrightRight = 0, stripBrightCenter = 0, stripBrightLeft = 0;

// Pellet / motor state (ported from FED4_Feed.cpp + FED4_Motor.cpp)
bool dropSensorAvailable = false;
bool feedBusy = false;
bool stripAnimBusy = false;
uint32_t motorTurns = 0;

// ISR-shared state (set in interrupt context, consumed in loop).
// ISRs must never do I2C — MCP23017 writes happen in loop context only.
volatile bool b3Pressed = false;
volatile uint32_t b3EdgeMs = 0;
volatile bool b2Level = false;      // BUTTON_2 level, tracked on CHANGE
volatile bool intOrLevelLow = false; // INT_OR line, tracked on CHANGE
volatile bool touchActiveL = false, touchActiveC = false, touchActiveR = false;

// Poke edge tracking — software baseline rise (same idle as LEDs), not ISR.
bool touchPokePrevL = false, touchPokePrevR = false, touchPokePrevC = false;
unsigned long pokeReadyMs = 0;

bool displayLedOn = true;
bool displayDirty = true;

// Non-blocking sensor state
bool tofRanging = false;
bool bmePending = false;
unsigned long bmeReadyMs = 0;

unsigned long lastPollMs = 0;
unsigned long lastOrientMs = 0;
unsigned long lastStripMs = 0;
unsigned long lastTofMs = 0;
unsigned long lastVcomMs = 0;

// ---------------------------------------------------------------------------
// VCOM keepalive (between display refreshes)
// ---------------------------------------------------------------------------

void vcomKeepAlive() {
  if (millis() - lastVcomMs >= 500) {
    lastVcomMs = millis();
    static bool vcom = false;
    vcom = !vcom;
    digitalWrite(DISPLAY_VCOM, vcom ? HIGH : LOW);
  }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void displayReset() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  pinMode(DISPLAY_VCOM, OUTPUT);
  digitalWrite(DISPLAY_VCOM, LOW);
  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
  displayLedOn = on;
  telem.dispLedOn = on;
}

void drawGateDot(int16_t x, int16_t y, bool blocked) {
  if (blocked)
    display.fillCircle(x, y, 3, PIXEL_WHITE);
  else
    display.drawCircle(x, y, 3, PIXEL_WHITE);
}

// GFX FreeFont cursor Y is the baseline; default font Y is the top edge.
static const int16_t HEADER_H = 20;
static const int16_t CONTENT_TOP = 28;
static const int16_t LABEL_BASELINE = 12; // offset from section top to 9pt baseline
static const int16_t LABEL_BLOCK_H = 15;
static const int16_t DATA_LINE_H = 10;
static const int16_t SECTION_GAP = 5;
static const int16_t FOOTER_Y = 302;
static const int16_t FOOTER_TEXT_Y = FOOTER_Y + 5;

void drawSectionLabel(int16_t y, const char *label) {
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(4, y + LABEL_BASELINE);
  display.print(label);
}

void drawDashboard() {
  display.clearBlack();

  // Header bar
  display.fillRect(0, 0, display.width(), HEADER_H, PIXEL_WHITE);
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(PIXEL_BLACK);
  display.setCursor(4, 13);
  if (telem.rtcOk)
    display.printf("%02u:%02u:%02u", telem.hour, telem.minute, telem.second);
  else
    display.print("--:--:--");

  display.setCursor(92, 13);
  if (telem.batReady && !isnan(telem.voltage) && telem.voltage > 0.0f)
    display.printf("%.2fV", telem.voltage);
  else
    display.print("--V");

  display.setCursor(138, 13);
  if (telem.batReady && !isnan(telem.percent) && telem.percent >= 0.0f)
    display.printf("%.0f%%", telem.percent);
  else
    display.print("--%");

  int16_t y = CONTENT_TOP;

  // ENV
  drawSectionLabel(y, "ENV");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(4, y);
  if (telem.bmeOk)
    display.printf("%.1fC %.0f%% %.0fhPa", telem.tempC, telem.humidity, telem.pressureHpa);
  else
    display.print("BME680 --");
  y += DATA_LINE_H;
  display.setCursor(4, y);
  if (telem.luxOk && !isnan(telem.lux) && telem.lux >= 0.0f)
    display.printf("%.1f lux", telem.lux);
  else
    display.print("LUX --");
  y += DATA_LINE_H + SECTION_GAP;

  // DIST
  drawSectionLabel(y, "DIST");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(2);
  display.setCursor(4, y);
  if (telem.tofOk && telem.tofMm > 0)
    display.printf("%d", telem.tofMm);
  else
    display.print("--");
  display.setTextSize(1);
  display.print(" mm");
  y += 18 + SECTION_GAP;

  // GATES
  drawSectionLabel(y, "GATES");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setCursor(4, y);
  display.print("C");
  display.setCursor(22, y);
  display.print("L");
  display.setCursor(40, y);
  display.print("R");
  display.setCursor(58, y);
  display.print("P");
  drawGateDot(12, y + 10, telem.pgC);
  drawGateDot(30, y + 10, telem.pgL);
  drawGateDot(48, y + 10, telem.pgR);
  drawGateDot(66, y + 10, telem.pgP);
  y += DATA_LINE_H + 10 + SECTION_GAP;

  // TOUCH
  drawSectionLabel(y, "TOUCH");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setCursor(4, y);
  display.printf("L:%lu C:%lu R:%lu", (unsigned long)telem.touchL,
                 (unsigned long)telem.touchC, (unsigned long)telem.touchR);
  y += DATA_LINE_H + SECTION_GAP;

  // POKE
  drawSectionLabel(y, "POKE");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setCursor(4, y);
  display.printf("Pokes: %lu", (unsigned long)telem.pokeCount);
  y += DATA_LINE_H;
  display.setCursor(4, y);
  display.printf("Pellets: %lu", (unsigned long)telem.pelletCount);
  if (feedBusy)
    display.print(" BUSY");
  y += DATA_LINE_H + SECTION_GAP;

  // ACCEL
  drawSectionLabel(y, "ACCEL");
  y += LABEL_BLOCK_H;
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setCursor(4, y);
  if (telem.accelOk)
    display.printf("X%+.1f Y%+.1f Z%+.1f", telem.accelX, telem.accelY, telem.accelZ);
  else
    display.print("LIS2DH --");
  y += DATA_LINE_H + SECTION_GAP;

  display.setCursor(4, y);
  display.printf("PIR:%s", telem.pirHigh ? "HI" : "LO");
  display.setCursor(72, y);
  display.printf("INT:%s", telem.intOrLow ? "LO" : "HI");

  // Footer
  display.fillRect(0, FOOTER_Y, display.width(), 18, PIXEL_WHITE);
  display.setTextColor(PIXEL_BLACK);
  display.setCursor(4, FOOTER_TEXT_Y);
  display.print("B1:BL B2:hap B3:tone");
  display.setCursor(152, FOOTER_TEXT_Y);
  display.print(displayLedOn ? "ON" : "OFF");

  display.drawRect(0, 0, display.width() - 1, display.height() - 1, PIXEL_WHITE);
}

void refreshDisplay() {
  display.refresh();
  lastVcomMs = millis();
}

// Device X (g) → MIP rotation; native at boot, 180° flip when X < 0.
bool orientDemoScreen() {
  if (!telem.accelOk)
    return false;

  sensors_event_t event;
  if (!accel.getEvent(&event))
    return false;

  telem.accelX = fed4DeviceAccelXG(event);
  telem.accelY = event.acceleration.x / FED4_GRAVITY_MS2;
  telem.accelZ = event.acceleration.z / FED4_GRAVITY_MS2;

  const uint8_t rot = fed4DisplayRotationForAccelX(telem.accelX);
  if (display.getRotation() == rot)
    return false;

  display.setRotation(rot);
  display.clearBlack();
  return true;
}

// ---------------------------------------------------------------------------
// STATUS_LED mirrors PIR_MOTION (see FED4-PIR-Sensor.ino)
// ---------------------------------------------------------------------------

void serviceStatusLed() {
  analogWrite(STATUS_LED, digitalRead(PIR_MOTION) ? 255 : 0);
}

// ---------------------------------------------------------------------------
// Touch — S3 hardware threshold interrupts + robust boot baseline
// ESP32-S3: counts RISE when touched; values are uint32_t (idle ~90k-170k).
// The touch FSM scans continuously in hardware; press/release interrupts set
// touchActive* flags. Raw values are only polled while a pad is active (for
// proportional LED brightness) and at the 500 ms telemetry poll (screen).
//
// FUTURE DEV (drift): the hardware benchmark auto-tracks slow drift, so the
// interrupt threshold stays valid. Consider periodically logging
// touch_pad_read_benchmark() against the boot idle average to detect/log
// drift, and re-running the baseline + threshold setup on large deviation.
// ---------------------------------------------------------------------------

void IRAM_ATTR onTouchLeft() {
  touchActiveL = touchInterruptGetLastStatus(TOUCH_PAD_LEFT);
}
void IRAM_ATTR onTouchCenter() {
  touchActiveC = touchInterruptGetLastStatus(TOUCH_PAD_CENTER);
}
void IRAM_ATTR onTouchRight() {
  touchActiveR = touchInterruptGetLastStatus(TOUCH_PAD_RIGHT);
}

// Average with min/max trimmed — one touched/glitched sample can't skew the
// baseline captured at reset.
uint32_t robustIdleAverage(touch_pad_t pad) {
  const int samples = 16;
  uint64_t sum = 0;
  uint32_t lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < samples; i++) {
    uint32_t v = touchRead(pad);
    sum += v;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
    delay(5);
  }
  return (uint32_t)((sum - lo - hi) / (samples - 2));
}

bool initTouchPads() {
  // First reads auto-initialize the touch peripheral
  touchRead(TOUCH_PAD_LEFT);
  touchRead(TOUCH_PAD_CENTER);
  touchRead(TOUCH_PAD_RIGHT);

  // Longer measurement window = better SNR for weak coupling (mouse nose)
  touchSetCycles(TOUCH_MEASURE_CYCLES, TOUCH_SLEEP_CYCLES);

  // Wider charge/discharge voltage window = larger swing per measurement
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);

  // Hardware denoise via internal reference channel (T0)
  touch_pad_denoise_t denoise = {
      .grade = TOUCH_PAD_DENOISE_BIT4,
      .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
  };
  touch_pad_denoise_set_config(&denoise);
  touch_pad_denoise_enable();

  // Hardware IIR filter for stable readings
  touch_filter_config_t filter = {
      .mode = TOUCH_PAD_FILTER_IIR_16,
      .debounce_cnt = 1,
      .noise_thr = 0,
      .jitter_step = 4,
      .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
  };
  touch_pad_filter_set_config(&filter);
  touch_pad_filter_enable();

  // Warm up after reconfiguration (first reads settle)
  for (int i = 0; i < 8; i++) {
    touchRead(TOUCH_PAD_LEFT);
    delay(5);
    touchRead(TOUCH_PAD_CENTER);
    delay(5);
    touchRead(TOUCH_PAD_RIGHT);
    delay(5);
  }

  // Baseline at reset — keep pads clear during boot
  touchIdleL = robustIdleAverage(TOUCH_PAD_LEFT);
  touchIdleC = robustIdleAverage(TOUCH_PAD_CENTER);
  touchIdleR = robustIdleAverage(TOUCH_PAD_RIGHT);

  bool ok = touchIdleL > 0 && touchIdleC > 0 && touchIdleR > 0;
  Serial.printf("Touch idle L:%lu C:%lu R:%lu (%s)\n",
                (unsigned long)touchIdleL, (unsigned long)touchIdleC,
                (unsigned long)touchIdleR, ok ? "OK" : "FAIL");
  if (!ok) return false;

  // S3 counts RISE on touch — threshold is idle + rise fraction (not idle * fraction).
  touchThreshL = touchIdleL + (uint32_t)(touchIdleL * TOUCH_TRIGGER_RISE);
  touchThreshC = touchIdleC + (uint32_t)(touchIdleC * TOUCH_TRIGGER_RISE);
  touchThreshR = touchIdleR + (uint32_t)(touchIdleR * TOUCH_TRIGGER_RISE);
  Serial.printf("Touch thr L:%lu C:%lu R:%lu\n",
                (unsigned long)touchThreshL, (unsigned long)touchThreshC,
                (unsigned long)touchThreshR);

  touchAttachInterrupt(TOUCH_PAD_LEFT, onTouchLeft, touchThreshL);
  touchAttachInterrupt(TOUCH_PAD_CENTER, onTouchCenter, touchThreshC);
  touchAttachInterrupt(TOUCH_PAD_RIGHT, onTouchRight, touchThreshR);

  // Sync ISR flags to hardware (clears spurious press state from attach).
  touchActiveL = touchInterruptGetLastStatus(TOUCH_PAD_LEFT);
  touchActiveC = touchInterruptGetLastStatus(TOUCH_PAD_CENTER);
  touchActiveR = touchInterruptGetLastStatus(TOUCH_PAD_RIGHT);
  return true;
}

void armPokeDetection() {
  readTouchPads();
  touchPokePrevL = touchPadAbovePokeThreshold(telem.touchL, touchIdleL);
  touchPokePrevR = touchPadAbovePokeThreshold(telem.touchR, touchIdleR);
  touchPokePrevC = touchPadAbovePokeThreshold(telem.touchC, touchIdleC);
  pokeReadyMs = millis();
}

bool pokeDetectionReady() {
  return millis() - pokeReadyMs >= POKE_ARM_MS;
}

static float touchRiseFraction(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

bool touchPadAbovePokeThreshold(uint32_t raw, uint32_t idle) {
  return touchRiseFraction(raw, idle) >= TOUCH_TRIGGER_RISE;
}

void readTouchPads() {
  telem.touchL = touchRead(TOUCH_PAD_LEFT);
  delayMicroseconds(800);
  telem.touchC = touchRead(TOUCH_PAD_CENTER);
  delayMicroseconds(800);
  telem.touchR = touchRead(TOUCH_PAD_RIGHT);
}

// S3: touch RAISES the count. Rise above idle maps to strip brightness.
uint8_t touchRiseToBrightness(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0;

  const float rise = (float)(raw - idle) / (float)idle;
  if (rise < TOUCH_DEADBAND) return 0;

  const float span = TOUCH_LED_FULL_RISE - TOUCH_DEADBAND;
  float t = (rise - TOUCH_DEADBAND) / span;
  if (t > 1.0f) t = 1.0f;
  // Slight gamma so mid-range touches feel more responsive.
  t = powf(t, 0.75f);

  uint8_t bright = (uint8_t)(t * 255.0f);
  if (bright > 0 && bright < TOUCH_LED_MIN_BRIGHT)
    bright = TOUCH_LED_MIN_BRIGHT;
  return bright;
}

uint8_t emaBrightness(uint8_t current, uint8_t target) {
  if (target > current)
    return (uint8_t)((current + target * 3) / 4);
  return (uint8_t)((current * 3 + target) / 4);
}

uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  return strip.Color(r * bright / 255, g * bright / 255, b * bright / 255);
}

uint32_t stripColorWheel(byte wheelPos) {
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85)
    return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  if (wheelPos < 170) {
    wheelPos -= 85;
    return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

// Full-strip rainbow theater chase while the center pad is touched.
void updateCenterRainbowDemo() {
  static uint8_t hue = 0;
  hue += 4;
  for (int i = 0; i < NUM_STRIP_LEDS; i++)
    strip.setPixelColor(i, stripColorWheel((uint8_t)(hue + i * 32)));
  strip.show();
}

bool centerPadTouched() {
  return touchRiseFraction(telem.touchC, touchIdleC) >= TOUCH_DEADBAND;
}

// Animation tick: poll raw rise from boot baseline every STRIP_MS (not ISR-gated).
void updateTouchStrip() {
  if (feedBusy || stripAnimBusy) return;

  telem.touchL = touchRead(TOUCH_PAD_LEFT);
  telem.touchC = touchRead(TOUCH_PAD_CENTER);
  telem.touchR = touchRead(TOUCH_PAD_RIGHT);

  if (centerPadTouched()) {
    updateCenterRainbowDemo();
    return;
  }

  const uint8_t targetL = touchRiseToBrightness(telem.touchL, touchIdleL);
  const uint8_t targetC = touchRiseToBrightness(telem.touchC, touchIdleC);
  const uint8_t targetR = touchRiseToBrightness(telem.touchR, touchIdleR);

  stripBrightRight = emaBrightness(stripBrightRight, targetR);
  stripBrightCenter = emaBrightness(stripBrightCenter, targetC);
  stripBrightLeft = emaBrightness(stripBrightLeft, targetL);

  // 0-2 right (blue), 3-4 center (green), 5-7 left (red) — touch glow only.
  for (int i = 0; i < 3; i++)
    strip.setPixelColor(i, scaleColor(0, 0, 255, stripBrightRight));
  for (int i = 3; i < 5; i++)
    strip.setPixelColor(i, scaleColor(0, 255, 0, stripBrightCenter));
  for (int i = 5; i < 8; i++)
    strip.setPixelColor(i, scaleColor(255, 0, 0, stripBrightLeft));
  strip.show();
}

// ---------------------------------------------------------------------------
// Audio — non-blocking chunked I2S sequencer
// A queue of parallel {freq, ms} arrays (freq 0 = silence). serviceAudio()
// writes one ~5 ms chunk per call; while playing, i2s.write() paces the loop
// to real time but never blocks longer than one chunk.
// (Parallel arrays instead of a struct: the Arduino builder emits function
// prototypes above type definitions, so custom types can't appear in
// .ino function signatures.)
// ---------------------------------------------------------------------------

static const uint32_t AUDIO_RATE = 48000;
static const size_t AUDIO_CHUNK = 256; // samples (~5.3 ms)
static const float AUDIO_AMPLITUDE = 0.22f;
static const int AUDIO_CHUNKS_PER_SERVICE = 12; // ~64 ms per serviceAudio() call
static const int AUDIO_PREFILL_CHUNKS = 8;    // prime DMA right after amp enable
static const uint32_t AUDIO_FADE_MS = 3;       // attack/release per note segment

static const int AUDIO_MAX_NOTES = 16;
uint16_t audioQueueFreq[AUDIO_MAX_NOTES]; // 0 = silence
uint16_t audioQueueDur[AUDIO_MAX_NOTES];  // ms
int audioQueueLen = 0;
int audioNoteIdx = 0;
uint32_t audioSamplesLeft = 0;
uint32_t audioNoteTotalSamples = 0;
uint32_t audioNoteSamplesDone = 0;
float audioPhase = 0.0f;
bool audioActive = false;

static void audioBeginNote() {
  audioNoteTotalSamples = audioSamplesLeft;
  audioNoteSamplesDone = 0;
}

static void audioAdvanceNote() {
  audioNoteIdx++;
  if (audioNoteIdx >= audioQueueLen) return;
  audioSamplesLeft = (AUDIO_RATE * audioQueueDur[audioNoteIdx]) / 1000;
  audioPhase = 0.0f;
  audioBeginNote();
}

static float audioEnvelope() {
  if (audioNoteTotalSamples == 0) return 1.0f;
  uint32_t fadeSamples = (AUDIO_RATE * AUDIO_FADE_MS) / 1000;
  if (fadeSamples > audioNoteTotalSamples / 4)
    fadeSamples = audioNoteTotalSamples / 4;
  if (fadeSamples < 1) return 1.0f;

  if (audioNoteSamplesDone < fadeSamples)
    return (float)audioNoteSamplesDone / (float)fadeSamples;
  if (audioNoteSamplesDone + fadeSamples > audioNoteTotalSamples)
    return (float)(audioNoteTotalSamples - audioNoteSamplesDone) / (float)fadeSamples;
  return 1.0f;
}

static size_t audioFillChunk(int16_t *buf) {
  size_t n = 0;
  while (n < AUDIO_CHUNK) {
    if (audioSamplesLeft == 0) {
      audioAdvanceNote();
      if (audioNoteIdx >= audioQueueLen) break;
    }

    uint16_t f = audioQueueFreq[audioNoteIdx];
    if (f == 0) {
      buf[n++] = 0;
    } else {
      float env = audioEnvelope();
      buf[n++] = (int16_t)(AUDIO_AMPLITUDE * env * sinf(audioPhase) * 32767.0f);
      audioPhase += 2.0f * (float)M_PI * (float)f / (float)AUDIO_RATE;
      if (audioPhase > 2.0f * (float)M_PI) audioPhase -= 2.0f * (float)M_PI;
    }
    audioSamplesLeft--;
    audioNoteSamplesDone++;
  }
  return n;
}

static void audioShutdownAmp() {
  int16_t silence[AUDIO_CHUNK] = {0};
  i2s.write((uint8_t *)silence, sizeof(silence));
  delayMicroseconds(500);
  mcp.digitalWrite(EXP_AMP_SD, LOW);
}

static int audioComputePrefillChunks() {
  uint32_t totalSamples = 0;
  for (int i = 0; i < audioQueueLen; i++)
    totalSamples += (AUDIO_RATE * audioQueueDur[i]) / 1000;
  int totalChunks = (totalSamples + AUDIO_CHUNK - 1) / AUDIO_CHUNK;
  int budget = totalChunks / 4;
  if (budget < 2) budget = 2;
  if (budget > AUDIO_PREFILL_CHUNKS) budget = AUDIO_PREFILL_CHUNKS;
  return budget;
}

static void audioPrimeOutput() {
  int16_t silence[AUDIO_CHUNK] = {0};
  i2s.write((uint8_t *)silence, sizeof(silence));
  i2s.write((uint8_t *)silence, sizeof(silence));
}

void audioStartQueue(const uint16_t *freqHz, const uint16_t *durMs, int count) {
  if (audioActive || count <= 0) return;
  if (count > AUDIO_MAX_NOTES) count = AUDIO_MAX_NOTES;
  memcpy(audioQueueFreq, freqHz, count * sizeof(uint16_t));
  memcpy(audioQueueDur, durMs, count * sizeof(uint16_t));
  audioQueueLen = count;
  audioNoteIdx = 0;
  audioSamplesLeft = (AUDIO_RATE * audioQueueDur[0]) / 1000;
  audioPhase = 0.0f;
  audioActive = true;
  audioBeginNote();
  audioPrimeOutput();
  mcp.digitalWrite(EXP_AMP_SD, HIGH);
  delay(5);
  audioWriteChunks(audioComputePrefillChunks());
}

static void audioWriteChunks(int maxChunks) {
  int16_t buf[AUDIO_CHUNK];
  for (int c = 0; c < maxChunks && audioActive; c++) {
    size_t n = audioFillChunk(buf);
    if (n > 0) i2s.write((uint8_t *)buf, n * sizeof(int16_t));
    if (audioNoteIdx >= audioQueueLen) {
      audioActive = false;
      audioShutdownAmp();
    }
  }
}

void serviceAudio() {
  if (!audioActive) return;
  audioWriteChunks(AUDIO_CHUNKS_PER_SERVICE);
}

void startMelody() {
  // Leading silence lets the amp settle after SD enable (avoids pop)
  const uint16_t freqs[] = {0, 523, 0, 659, 0, 784, 0, 988, 0, 1175, 0, 1319, 0, 1568};
  const uint16_t durs[] = {5, 120, 30, 120, 30, 120, 30, 120, 30, 120, 30, 120, 30, 280};
  audioStartQueue(freqs, durs, sizeof(freqs) / sizeof(freqs[0]));
}

void startChirp() {
  // Longer leading silence than melody: short clips need ≥1 chunk of settle
  // after amp enable so the tone does not start mid-buffer with a pop.
  const uint16_t freqs[] = {0, 1200};
  const uint16_t durs[] = {15, 45};
  audioStartQueue(freqs, durs, sizeof(freqs) / sizeof(freqs[0]));
}

static const uint16_t POKE_TONE_LEFT_HZ = 5000;  // high — left poke
static const uint16_t POKE_TONE_RIGHT_HZ = 2000; // low — right poke
static const uint16_t POKE_TONE_MS = 100;

void audioWaitUntilDone() {
  while (audioActive)
    cooperativeYield();
}

void playPokeTone(bool leftPoke) {
  audioWaitUntilDone();

  const uint16_t freq = leftPoke ? POKE_TONE_LEFT_HZ : POKE_TONE_RIGHT_HZ;
  const uint16_t freqs[] = {0, freq};
  const uint16_t durs[] = {15, POKE_TONE_MS};
  audioStartQueue(freqs, durs, 2);
  audioWaitUntilDone();
}

// Blocking boot clip — PCM from PROGMEM, same I2S path as runtime audio.
void playStartupSound() {
  mcp.digitalWrite(EXP_AMP_SD, HIGH);
  delay(5);

  int16_t buf[AUDIO_CHUNK];
  for (size_t i = 0; i < STARTUP_PCM_SAMPLES;) {
    size_t n = min(AUDIO_CHUNK, STARTUP_PCM_SAMPLES - i);
    memcpy_P(buf, &STARTUP_PCM[i], n * sizeof(int16_t));
    i2s.write((uint8_t *)buf, n * sizeof(int16_t));
    i += n;
  }

  mcp.digitalWrite(EXP_AMP_SD, LOW);
}

// ---------------------------------------------------------------------------
// GPIO ISRs — set flags/levels only; work happens in loop context
// ---------------------------------------------------------------------------

void IRAM_ATTR onButton2() {
  b2Level = (digitalRead(BUTTON_2) == HIGH);
}

void IRAM_ATTR onButton3() {
  uint32_t now = millis();
  if (now - b3EdgeMs > BUTTON_DEBOUNCE_MS) {
    b3EdgeMs = now;
    b3Pressed = true;
  }
}

// Track the ORed interrupt line state by interrupt instead of polling.
// FUTURE DEV: on assertion, query each source behind INT_OR (LIS2DH INT,
// MCP23017 INTA/B, RTC alarm) to identify and clear the interrupt.
void IRAM_ATTR onIntOr() {
  intOrLevelLow = (digitalRead(INT_OR) == LOW);
}

// ---------------------------------------------------------------------------
// Event consumers (loop context — safe for I2C)
// ---------------------------------------------------------------------------

// B1: polled debounce, toggle backlight on debounced release (one click = one toggle).
void serviceButton1Backlight() {
  static bool lastReading = false;
  static bool stableHigh = false;
  static uint32_t lastChangeMs = 0;

  const bool reading = (digitalRead(BUTTON_1) == HIGH);
  const uint32_t now = millis();

  if (reading != lastReading) {
    lastChangeMs = now;
    lastReading = reading;
  }

  if (now - lastChangeMs < BUTTON_DEBOUNCE_MS)
    return;

  if (reading == stableHigh)
    return;

  const bool wasHigh = stableHigh;
  stableHigh = reading;

  if (wasHigh && !stableHigh) {
    displayLedOn = !displayLedOn;
    displayLight(displayLedOn);
    displayDirty = true;
  }
}

void serviceButtons() {
  serviceButton1Backlight();
  if (b3Pressed) {
    b3Pressed = false;
    if (!audioActive) startMelody();
  }
}

// Haptic follows B2 level; MCP written only on change (I2C from loop only)
void serviceHaptic() {
  static bool applied = false;
  bool level = b2Level;
  if (level != applied) {
    applied = level;
    mcp.digitalWrite(EXP_HAPTIC, level ? HIGH : LOW);
  }
}

void pokeHapticPulse(uint16_t ms = 80) {
  mcp.digitalWrite(EXP_HAPTIC, HIGH);
  delayCooperative(ms);
  mcp.digitalWrite(EXP_HAPTIC, b2Level ? HIGH : LOW);
}

void cooperativeYield() {
  serviceAudio();
  serviceButtons();
  serviceHaptic();
}

void delayCooperative(uint32_t ms) {
  const unsigned long start = millis();
  while (millis() - start < ms) {
    cooperativeYield();
    delay(1);
  }
}

// ---------------------------------------------------------------------------
// Motor + pellet dispense (ported from FED4_Motor.cpp / FED4_Feed.cpp)
// ---------------------------------------------------------------------------

void releaseMotor() {
  digitalWrite(MOTOR_PIN_1, LOW);
  digitalWrite(MOTOR_PIN_2, LOW);
  digitalWrite(MOTOR_PIN_3, LOW);
  digitalWrite(MOTOR_PIN_4, LOW);
}

void minorJamClear() {
  Serial.println("MinorJam");
  stepper.step(200);
  delayCooperative(1000);
}

void vibrateJamClear() {
  Serial.println("VibrateJam");
  for (int i = 0; i < 35; i++) {
    stepper.step(10);
    delayCooperative(10);
    stepper.step(-20);
    delayCooperative(10);
  }
}

void handleDemoJams() {
  if (motorTurns % 100 == 0)
    minorJamClear();
  if (motorTurns % 200 == 0)
    vibrateJamClear();
  if (motorTurns > 2000) {
    Serial.println("Dispense timeout (jam)");
    releaseMotor();
  }
}

bool checkForPellet() {
  return digitalRead(PHOTOGATE_1) == LOW;
}

bool didPelletDrop() {
  if (dropSensorAvailable)
    return digitalRead(PHOTOGATE_4) == LOW;
  return false;
}

bool initializeDropSensor() {
  dropSensorAvailable = (digitalRead(PHOTOGATE_4) == HIGH);
  return dropSensorAvailable;
}

bool initMotor() {
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);
  releaseMotor();
  stepper.setSpeed(MOTOR_SPEED_RPM);
  Serial.println("OK: Motor (28BYJ-48, 2048 steps/rev)");
  return true;
}

void demoDispense() {
  bool pelletPresent = checkForPellet();
  bool pelletDropped = false;

  while (!pelletPresent && !pelletDropped && motorTurns <= 2000) {
    pelletDropped = didPelletDrop();
    pelletPresent = checkForPellet();

    stepper.step(-10);
    cooperativeYield();
    delay(2);
    motorTurns++;

    if (motorTurns % 25 == 0) {
      Serial.print("Dispensing... ");
      Serial.println(motorTurns / 25);
      releaseMotor();
      delayCooperative(1000);
    }

    handleDemoJams();
    pelletPresent = checkForPellet();
    pelletDropped = didPelletDrop();
  }

  releaseMotor();
}

void demoWaitForPelletSettling() {
  const unsigned long startWait = millis();
  while (millis() - startWait < 500) {
    if (checkForPellet())
      return;
    delayCooperative(10);
  }
}

void stripColorWipe(uint8_t r, uint8_t g, uint8_t b, uint16_t stepMs = 30) {
  stripAnimBusy = true;
  for (int i = 0; i < NUM_STRIP_LEDS; i++) {
    strip.clear();
    strip.setPixelColor(i, strip.Color(r, g, b));
    strip.show();
    delayCooperative(stepMs);
  }
  delayCooperative(stepMs * 2);
  strip.clear();
  strip.show();
  stripAnimBusy = false;
}

void serviceFeed() {
  if (!pokeDetectionReady() || feedBusy)
    return;

  readTouchPads();

  const bool lNow = touchPadAbovePokeThreshold(telem.touchL, touchIdleL);
  const bool rNow = touchPadAbovePokeThreshold(telem.touchR, touchIdleR);
  const bool lPoke = lNow && !touchPokePrevL;
  const bool rPoke = rNow && !touchPokePrevR;
  touchPokePrevL = lNow;
  touchPokePrevR = rNow;

  if (!lPoke && !rPoke)
    return;

  bool isLeft;
  if (lPoke && rPoke) {
    // Crosstalk — use whichever pad rose more above baseline.
    isLeft = touchRiseFraction(telem.touchL, touchIdleL) >=
             touchRiseFraction(telem.touchR, touchIdleR);
  } else {
    isLeft = lPoke;
  }

  telem.pokeCount++;
  feedBusy = true;
  displayDirty = true;
  Serial.println(isLeft ? "Poke LEFT — dispense" : "Poke RIGHT — dispense");

  pokeHapticPulse();
  playPokeTone(isLeft);
  demoDispense();
  demoWaitForPelletSettling();

  // Green wipe only when the center-well photogate sees the pellet.
  if (checkForPellet()) {
    telem.pelletCount++;
    Serial.println("Pellet gate — LED wipe");
    stripColorWipe(180, 255, 180,
                   PELLET_WIPE_MS / (NUM_STRIP_LEDS + 2));
  } else {
    Serial.println("Pellet not detected in well");
  }

  motorTurns = 0;
  feedBusy = false;
  displayDirty = true;
}

void serviceCenterPoke() {
  if (!pokeDetectionReady() || feedBusy || stripAnimBusy || audioActive)
    return;

  readTouchPads();

  const bool cNow = touchPadAbovePokeThreshold(telem.touchC, touchIdleC);
  const bool cPoke = cNow && !touchPokePrevC;
  touchPokePrevC = cNow;

  if (!cPoke)
    return;

  telem.pokeCount++;
  feedBusy = true;
  displayDirty = true;
  Serial.println("Poke CENTER — intro");

  pokeHapticPulse();
  audioWaitUntilDone();
  playStartupSound();

  feedBusy = false;
  displayDirty = true;
}

// ---------------------------------------------------------------------------
// Non-blocking sensor services
// ---------------------------------------------------------------------------

// ToF runs in continuous ranging mode; just harvest results when ready
void serviceToF() {
  if (!tofRanging) return;
  if (millis() - lastTofMs < TOF_CHECK_MS) return;
  lastTofMs = millis();

  if (!tofSensor.checkForDataReady()) return;
  telem.tofMm = tofSensor.getDistance();
  tofSensor.clearInterrupt(); // re-arm for the next sample
}

// BME680 async: beginReading() kicks off the (slow) gas measurement;
// endReading() harvests it once ready — no blocking wait
void serviceBme() {
  if (!bmePending || millis() < bmeReadyMs) return;
  bmePending = false;
  if (bme.endReading()) {
    telem.tempC = bme.temperature;
    telem.humidity = bme.humidity;
    telem.pressureHpa = bme.pressure / 100.0f;
    telem.gasKOhm = bme.gas_resistance / 1000.0f;
  }
}

// ---------------------------------------------------------------------------
// Sensor polling (500 ms) — screen-only values; no blocking calls
// ---------------------------------------------------------------------------

void pollSensors() {
  if (telem.rtcOk) {
    DateTime now = rtc.now();
    telem.hour = now.hour();
    telem.minute = now.minute();
    telem.second = now.second();
  }

  telem.batReady = telem.batOk && maxlipo.isDeviceReady();
  if (telem.batReady) {
    telem.voltage = maxlipo.cellVoltage();
    telem.percent = maxlipo.cellPercent();
  } else {
    telem.voltage = NAN;
    telem.percent = NAN;
  }

  if (telem.bmeOk && !bmePending && !feedBusy) {
    unsigned long readyAt = bme.beginReading();
    if (readyAt != 0) {
      bmeReadyMs = readyAt;
      bmePending = true;
    }
  }

  if (telem.luxOk) {
    float lux = veml.readLux();
    telem.lux = (isnan(lux) || lux < 0.0f) ? NAN : lux;
  }

  telem.pgC = (digitalRead(PHOTOGATE_1) == LOW);
  telem.pgL = (digitalRead(PHOTOGATE_2) == LOW);
  telem.pgR = (digitalRead(PHOTOGATE_3) == LOW);
  telem.pgP = (digitalRead(PHOTOGATE_4) == LOW);

  readTouchPads();

  if (telem.accelOk) {
    sensors_event_t event;
    if (accel.getEvent(&event)) {
      telem.accelX = fed4DeviceAccelXG(event);
      telem.accelY = event.acceleration.x / FED4_GRAVITY_MS2;
      telem.accelZ = event.acceleration.z / FED4_GRAVITY_MS2;
    }
  }

  telem.pirHigh = (digitalRead(PIR_MOTION) == HIGH);
  telem.intOrLow = intOrLevelLow;

  displayDirty = true;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool initSensors() {
  telem.bmeOk = bme.begin(I2C_ADDR_BME680, &Wire);
  if (telem.bmeOk) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("OK: BME680");
  } else {
    Serial.println("WARN: BME680");
  }

  telem.luxOk = veml.begin(&Wire);
  if (telem.luxOk) {
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_100MS); // 800MS blocked the loop ~500ms+
    veml.powerSaveEnable(false);
    veml.enable(true);
    Serial.println("OK: VEML7700");
  } else {
    Serial.println("WARN: VEML7700");
  }

  telem.tofOk = (tofSensor.begin() == 0);
  Serial.println(telem.tofOk ? "OK: VL53L1X" : "WARN: VL53L1X");

  telem.rtcOk = rtc.begin(&Wire);
  if (telem.rtcOk) {
    if (rtc.lostPower())
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("OK: RTC");
  } else {
    Serial.println("WARN: RTC");
  }

  // MAX17048 is on VBATT only. begin() issues a POR reset that NACKs by design;
  // ESP32 core 3.x may log one i2c.master error — harmless if init succeeds.
  telem.batOk = maxlipo.begin();
  telem.batReady = telem.batOk && maxlipo.isDeviceReady();
  if (!telem.batOk) {
    Serial.println("WARN: MAX17048 (no I2C — check VBATT power)");
  } else if (!telem.batReady) {
    Serial.println("OK: MAX17048 (chip only — gauge not ready, no pack?)");
  } else {
    Serial.println("OK: MAX17048");
  }

  telem.accelOk = accel.begin(I2C_ADDR_ACCEL);
  if (telem.accelOk) {
    accel.setRange(LIS3DH_RANGE_2_G);
    accel.setDataRate(LIS3DH_DATARATE_50_HZ);
    accel.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);
    Serial.println("OK: LIS2DH");
  } else {
    Serial.println("WARN: LIS2DH");
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== FED4 Demo Hardware ===");

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  if (!mcp.begin_I2C()) {
    Serial.println("FAIL: MCP23017");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW);
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delay(5);

  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_AMP_SD, LOW);
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  displayReset();
  displayLight(true);

  if (!display.begin()) {
    Serial.println("FAIL: display framebuffer");
    while (1) delay(10);
  }
  display.clearBlack();
  display.refresh();

  initSensors();

  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);
  pinMode(PHOTOGATE_1, INPUT_PULLUP);
  pinMode(PHOTOGATE_2, INPUT_PULLUP);
  pinMode(PHOTOGATE_3, INPUT_PULLUP);
  pinMode(PHOTOGATE_4, INPUT_PULLUP);
  pinMode(PIR_MOTION, INPUT_PULLDOWN);
  pinMode(INT_OR, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  analogWrite(STATUS_LED, 0);

  attachInterrupt(digitalPinToInterrupt(BUTTON_2), onButton2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3), onButton3, RISING);
  attachInterrupt(digitalPinToInterrupt(INT_OR), onIntOr, CHANGE);
  b2Level = (digitalRead(BUTTON_2) == HIGH);
  intOrLevelLow = (digitalRead(INT_OR) == LOW);

  i2s.setPins(AMP_BCLK, AMP_LRCLK, AMP_DIN);
  i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  playStartupSound();

  strip.begin();
  strip.setBrightness(120);
  strip.clear();
  strip.show();

  if (!initTouchPads())
    Serial.println("WARN: touch init — keep pads clear at boot");
  armPokeDetection();

  initMotor();
  if (initializeDropSensor())
    Serial.println("OK: Drop sensor (PHOTOGATE_4)");
  else
    Serial.println("WARN: Drop sensor absent or blocked at boot");

  // Start continuous ranging once; serviceToF() harvests results
  if (telem.tofOk) {
    tofSensor.startRanging();
    tofRanging = true;
  }

  lastStripMs = millis();
  lastPollMs = millis();
  lastOrientMs = millis();
  pollSensors();
  if (orientDemoScreen())
    displayDirty = true;
  Serial.println("Demo ready.");
}

void loop() {
  serviceButtons();
  serviceHaptic();
#if ENABLE_FEED
  serviceFeed();
  serviceCenterPoke();
#endif

  // While tones play, keep the loop I2S-heavy — display SPI and sensor
  // polls were starving DMA and causing crackle on the melody.
  if (audioActive) {
    serviceAudio();
    serviceStatusLed();
    serviceToF();
    serviceBme();
    vcomKeepAlive();
    serviceAudio();
    return;
  }

  serviceAudio();
  serviceStatusLed();
  serviceToF();
  serviceBme();
  vcomKeepAlive();

  if (millis() - lastStripMs >= STRIP_MS) {
    lastStripMs = millis();
    updateTouchStrip();
  }

  if (millis() - lastOrientMs >= ORIENT_MS) {
    lastOrientMs = millis();
    if (orientDemoScreen())
      displayDirty = true;
  }

  if (millis() - lastPollMs >= POLL_MS) {
    lastPollMs = millis();
    pollSensors();
  }

  if (displayDirty) {
    displayDirty = false;
    drawDashboard();
    refreshDisplay();
  }
}
