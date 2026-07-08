/*
 * FED4 Battery Test (MAX17048)
 *
 * Current hardware uses the MAX17048 fuel gauge on the main I2C bus.
 * This replaces legacy direct ADC battery measurement.
 *
 * Pins / address (see src/FED4_Pins.h):
 *   SDA=8, SCL=9, 100kHz
 *   MAX17048 at 0x36 (fixed)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX1704X.h>

#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_MAX17048 maxlipo;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Battery Test ===");
  Serial.println("MAX17048 on main I2C bus");

  Wire.begin(SDA_PIN, SCL_PIN, 100000); // Match FED4::begin()
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
  Serial.println();
}

void loop() {
  float voltage = maxlipo.cellVoltage();
  float percent = maxlipo.cellPercent();

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

  Serial.println();
  delay(1000);
}
