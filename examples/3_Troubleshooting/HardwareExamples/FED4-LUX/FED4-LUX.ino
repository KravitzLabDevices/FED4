/*
 * FED4 LUX Test (VEML7700)
 *
 * Updated for current hardware:
 * - Single main I2C bus: SDA=8, SCL=9
 * - VEML7700 on default address 0x10
 *
 * Settings mirror FED4 library defaults (src/FED4_Vitals.cpp):
 * - Gain: VEML7700_GAIN_2
 * - Integration: VEML7700_IT_800MS
 * - Power save disabled
 */

#include <Wire.h>
#include "Adafruit_VEML7700.h"

#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_VEML7700 veml;

void printSensorConfig() {
  Serial.print("Gain: ");
  switch (veml.getGain()) {
    case VEML7700_GAIN_1: Serial.println("1x"); break;
    case VEML7700_GAIN_2: Serial.println("2x"); break;
    case VEML7700_GAIN_1_4: Serial.println("1/4x"); break;
    case VEML7700_GAIN_1_8: Serial.println("1/8x"); break;
  }

  Serial.print("Integration Time (ms): ");
  switch (veml.getIntegrationTime()) {
    case VEML7700_IT_25MS: Serial.println("25"); break;
    case VEML7700_IT_50MS: Serial.println("50"); break;
    case VEML7700_IT_100MS: Serial.println("100"); break;
    case VEML7700_IT_200MS: Serial.println("200"); break;
    case VEML7700_IT_400MS: Serial.println("400"); break;
    case VEML7700_IT_800MS: Serial.println("800"); break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 LUX Test (VEML7700) ===");

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeout(1000);

  if (!veml.begin(&Wire)) {
    Serial.println("VEML7700 not found on main I2C bus.");
    while (1) delay(10);
  }

  // Match library defaults for dark-room sensitivity.
  veml.setGain(VEML7700_GAIN_2);
  veml.setIntegrationTime(VEML7700_IT_800MS);
  veml.powerSaveEnable(false);
  veml.enable(true);

  printSensorConfig();
  Serial.println("Sensor ready.\n");
}

void loop() {
  float lux = veml.readLux();
  uint16_t als = veml.readALS();
  uint16_t white = veml.readWhite();

  Serial.print("raw ALS: ");
  Serial.println(als);

  Serial.print("raw white: ");
  Serial.println(white);

  Serial.print("lux: ");
  if (isnan(lux) || lux < 0.0f) {
    Serial.println("invalid");
  } else {
    Serial.println(lux, 2);
  }

  Serial.println();
  delay(500);
}