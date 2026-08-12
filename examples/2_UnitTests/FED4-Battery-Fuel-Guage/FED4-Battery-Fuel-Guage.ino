/*
 * FED4 Battery Test (MAX17048)
 *
 * MAX17048 is powered from VBATT (not the always-on 3.3V rail).
 * MCP73831 charger: PROG = 2.5kΩ to GND (~500 mA fast charge).
 *
 * With USB present and no LiPo, VBATT can still sit near VREG (~4.2V)
 * on the bulk cap — the fuel gauge will report plausible voltage/SOC.
 * Cross-check MCP73831 STAT (no battery = high-Z) for true pack presence.
 *
 * Pins / address (see src/FED4_Pins.h):
 *   SDA=8, SCL=9, 100kHz
 *   MAX17048 at 0x36 (fixed)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX1704X.h>
#include <FED4_Pins.h>

Adafruit_MAX17048 maxlipo;

void printChipDiagnostics() {
  uint8_t chipId = maxlipo.getChipID();
  uint16_t version = maxlipo.getICversion();
  bool ready = maxlipo.isDeviceReady();

  Serial.printf("Chip ID: 0x%02X", chipId);
  if (chipId == 0xFF) {
    Serial.print(" (unpowered / no VBATT?)");
  }
  Serial.println();

  Serial.printf("IC version: 0x%04X", version);
  if (version == 0xFFFF) {
    Serial.print(" (unpowered / no VBATT?)");
  }
  Serial.println();

  Serial.print("isDeviceReady(): ");
  Serial.println(ready ? "true" : "false");
  if (!ready) {
    Serial.println("Gauge not ready — check VBATT power and I2C wiring.");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Battery Test ===");
  Serial.println("MAX17048 on main I2C, powered from VBATT");
  Serial.println("MCP73831 PROG=2.5kΩ (~500 mA charge)");

  Wire.begin(SDA, SCL, 100000); // Match FED4::begin()
  Wire.setTimeout(1000);

  const int maxRetries = 3;
  int retryCount = 0;
  bool initialized = false;

  while (!initialized && retryCount < maxRetries) {
    retryCount++;
    initialized = maxlipo.begin();
    if (!initialized) {
      Serial.printf("Battery monitor init attempt %d failed, retrying...\n", retryCount);
      delay(10);
    }
  }

  if (!initialized) {
    Serial.println("Battery monitor initialization failed. Freezing...");
    while (1) delay(10);
  }

  Serial.println("MAX17048 initialized.");
  printChipDiagnostics();
  Serial.println();
}

void loop() {
  if (!maxlipo.isDeviceReady()) {
    Serial.println("isDeviceReady(): false — skipping readings");
    Serial.println();
    delay(1000);
    return;
  }

  float voltage = maxlipo.cellVoltage();
  float percent = maxlipo.cellPercent();
  float rate = maxlipo.chargeRate(); // %/hour; + = charging, - = discharging

  Serial.print("Battery voltage: ");
  if (isnan(voltage) || voltage <= 0.0f || voltage > 5.0f) {
    Serial.println("invalid");
  } else {
    Serial.print(voltage, 3);
    Serial.println(" V");
  }

  Serial.print("Battery percent: ");
  if (isnan(percent) || percent < 0.0f || percent > 100.0f) {
    Serial.println("invalid");
  } else {
    Serial.print(percent, 1);
    Serial.println(" %");
  }

  Serial.print("Charge rate: ");
  if (isnan(rate)) {
    Serial.println("invalid");
  } else {
    Serial.print(rate, 2);
    Serial.println(" %/hr");
  }

  Serial.println();
  delay(1000);
}
