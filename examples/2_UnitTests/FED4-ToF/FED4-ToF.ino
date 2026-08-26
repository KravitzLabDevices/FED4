/*
 * FED4 ToF Distance Test (SparkFun VL53L1X)
 *
 * Matches FED4-Demo-Hardware v1.0.5 ToF path (no strip):
 *   Wire.setTimeOut(50), i2cBusHealthy restores Wire, short mode,
 *   4×4 ROI @ 199, continuous ranging, status==0, −20 mm offset.
 */

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "SparkFun_VL53L1X.h"
#include <FED4_Pins.h>

static const int16_t TOF_CALIBRATION_MM = 20;
static const uint8_t TOF_USE_NARROW_ROI = 1;
static const uint8_t TOF_ROI_WIDTH = 4;
static const uint8_t TOF_ROI_HEIGHT = 4;
static const uint8_t TOF_ROI_CENTER = 199;
static const uint32_t TOF_CHECK_MS = 25;

Adafruit_MCP23X17 mcp;
SFEVL53L1X tofSensor(Wire);

static uint32_t lastTofMs = 0;
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== FED4 ToF Distance Test ===");

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

  if (!i2cBusHealthy()) {
    Serial.println("WARN: I2C bus not idle — recovering");
    i2cRecoverBus();
  }

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
    while (1)
      delay(10);
  }
  Serial.println("OK");

  tofSensor.startRanging();
  tofRanging = true;
  lastTofMs = millis();
  Serial.println("Sensor online!");
}

void loop() {
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
    Serial.printf("invalid  status=%u raw=%d\n", (unsigned)status, rawMm);
    return;
  }

  int mm = rawMm - TOF_CALIBRATION_MM;
  if (mm < 0)
    mm = 0;

  Serial.print("Distance(mm): ");
  Serial.println(mm);
}
