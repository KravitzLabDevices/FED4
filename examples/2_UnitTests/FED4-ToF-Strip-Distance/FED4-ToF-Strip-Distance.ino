/*
 * FED4 ToF → Strip Distance Test
 *
 * VL53L1X distance drives front RGB strip brightness (blue).
 * No display / FED4::begin().
 *
 * I2C + ToF path matches FED4-Demo-Hardware v1.0.5:
 *   Wire.setTimeOut(50), MCP/rails, i2cBusHealthy (restores Wire),
 *   ToF begin+config before NeoPixel, startRanging last,
 *   short mode + 4×4 ROI @ 199, status==0 gate, −20 mm offset.
 *
 * Binding:
 *   distance < 30 mm  → max blue
 *   distance > 100 mm → min (off)
 *   30–100 mm         → linear interpolate
 *
 * Hardware: ToF always-on 3.3V (XSHUT pulled up). Strip on PSV3 (MCP).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include "SparkFun_VL53L1X.h"
#include <FED4_Pins.h>

static const int NUMPIXELS = 8;
static const int DIST_NEAR_MM = 30;
static const int DIST_FAR_MM = 100;
static const uint8_t BRIGHT_MAX = 255;
static const uint8_t BRIGHT_MIN = 0;

static const int16_t TOF_CALIBRATION_MM = 20;
static const uint8_t TOF_USE_NARROW_ROI = 1;
static const uint8_t TOF_ROI_WIDTH = 4;
static const uint8_t TOF_ROI_HEIGHT = 4;
static const uint8_t TOF_ROI_CENTER = 199;
static const uint32_t TOF_CHECK_MS = 25;

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel pixels(NUMPIXELS, RGB_STRIP, NEO_GRB + NEO_KHZ800);
SFEVL53L1X tofSensor(Wire);

static uint32_t lastTofMs = 0;
static uint8_t lastBlue = 0xFF;
static bool tofRanging = false;
static bool tofOk = false;

static bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission(true) == 0;
}

static void i2cRecoverBus() {
  Wire.end();
  pinMode(SCL, INPUT_PULLUP);
  pinMode(SDA, INPUT_PULLUP);
  delay(1);

  pinMode(SCL, OUTPUT);
  digitalWrite(SCL, HIGH);
  for (int i = 0; i < 9 && digitalRead(SDA) == LOW; i++) {
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
  }

  pinMode(SCL, INPUT_PULLUP);
  pinMode(SDA, INPUT_PULLUP);
  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeOut(50);
}

// Always restore Wire — pinMode(SDA/SCL) detaches the ESP32 I2C driver.
static bool i2cBusHealthy() {
  Wire.end();
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);
  const bool ok = digitalRead(SDA) == HIGH && digitalRead(SCL) == HIGH;
  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeOut(50);
  return ok;
}

static bool beginToFDevice() {
  if (tofSensor.begin() != 0)
    return false;

  tofSensor.setDistanceModeShort();
  tofSensor.setTimingBudgetInMs(50);
  tofSensor.setIntermeasurementPeriod(100);

  if (TOF_USE_NARROW_ROI)
    tofSensor.setROI(TOF_ROI_WIDTH, TOF_ROI_HEIGHT, TOF_ROI_CENTER);

  return true;
}

static uint8_t brightnessForDistanceMm(int mm) {
  if (mm < 0)
    return BRIGHT_MIN;
  if (mm <= DIST_NEAR_MM)
    return BRIGHT_MAX;
  if (mm >= DIST_FAR_MM)
    return BRIGHT_MIN;
  const int span = DIST_FAR_MM - DIST_NEAR_MM;
  const int t = mm - DIST_NEAR_MM;
  return (uint8_t)((BRIGHT_MAX * (span - t)) / span);
}

static void setStripBlue(uint8_t brightness) {
  if (brightness == lastBlue)
    return;
  lastBlue = brightness;
  const uint32_t c = pixels.Color(0, 0, brightness);
  for (int i = 0; i < pixels.numPixels(); i++)
    pixels.setPixelColor(i, c);
  pixels.show();
}

static void serviceToF() {
  if (!tofRanging)
    return;
  if (millis() - lastTofMs < TOF_CHECK_MS)
    return;
  lastTofMs = millis();

  if (!tofSensor.checkForDataReady())
    return;

  const uint8_t status = tofSensor.getRangeStatus();
  const int rawMm = (int)tofSensor.getDistance();
  tofSensor.clearInterrupt();

  if (status != 0) {
    setStripBlue(BRIGHT_MIN);
    Serial.printf("invalid  status=%u raw=%d\n", (unsigned)status, rawMm);
    return;
  }

  int mm = rawMm - TOF_CALIBRATION_MM;
  if (mm < 0)
    mm = 0;

  const uint8_t b = brightnessForDistanceMm(mm);
  setStripBlue(b);
  Serial.printf("Distance(mm): %d  blue=%u\n", mm, (unsigned)b);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== FED4 ToF Strip Distance ===");
  Serial.printf("Blue: max @ <%d mm, min @ >%d mm\n", DIST_NEAR_MM, DIST_FAR_MM);

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeOut(50);

  if (!mcp.begin_I2C()) {
    Serial.println("FAIL: MCP23017");
    while (1)
      delay(10);
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

  if (!i2cBusHealthy()) {
    Serial.println("WARN: I2C bus not idle — recovering");
    i2cRecoverBus();
  }

  // ToF begin+config before NeoPixel (Demo: initSensors before strip)
  Serial.printf("  VL53L1X @ 0x%02X... ", I2C_ADDR_TOF);
  Serial.flush();
  for (int attempt = 0; attempt < 2 && !tofOk; attempt++) {
    if (attempt > 0) {
      Serial.print("retry... ");
      Serial.flush();
      i2cRecoverBus();
    }
    if (!i2cProbe(I2C_ADDR_TOF))
      continue;
    if (!beginToFDevice())
      continue;
    tofOk = true;
  }

  if (!tofOk) {
    Serial.println("not on bus / begin failed");
    i2cRecoverBus();
    Serial.println("Freezing...");
    while (1)
      delay(10);
  }
  Serial.println("OK");

  // Strip after ToF config, before startRanging
  pixels.begin();
  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();

  tofSensor.startRanging();
  tofRanging = true;
  lastTofMs = millis();
  Serial.println("Ready (Demo ToF config + continuous ranging).");
}

void loop() {
  serviceToF();
}
