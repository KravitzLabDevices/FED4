/*
 * FED4 ToF Distance Test (SparkFun VL53L1X)
 *
 * Reads distance from the VL53L1X on the primary I2C bus.
 * On current hardware the sensor is always-on (no XSHUT pin).
 *
 * Pins / address (see src/FED4_Pins.h, src/FED4_Prox.cpp):
 *   SDA=8, SCL=9, 100kHz
 *   I2C address 0x29
 *
 * Library: SparkFun VL53L1X (same as FED4 library)
 */

#include <Wire.h>
#include "SparkFun_VL53L1X.h"  // http://librarymanager/All#SparkFun_VL53L1X
#include <FED4_Pins.h>

// No XSHUT — sensor is always powered
SFEVL53L1X tofSensor(Wire);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 ToF Distance Test ===");
  Serial.println("VL53L1X on main I2C (always-on, no XSHUT)");

  Wire.begin(SDA, SCL, 100000); // 100kHz — match FED4::initializeToF()
  Wire.setTimeout(1000);
  delay(10);

  if (tofSensor.begin() != 0) {  // begin() returns 0 on success
    Serial.println("ToF sensor failed to begin. Check wiring / power. Freezing...");
    while (1) delay(10);
  }

  Serial.println("Sensor online!");
  Serial.println();
}

void loop() {
  tofSensor.startRanging();

  unsigned long startTime = millis();
  while (!tofSensor.checkForDataReady()) {
    if (millis() - startTime > 100) {
      Serial.println("Timeout waiting for distance data");
      tofSensor.stopRanging();
      delay(500);
      return;
    }
    delay(1);
  }

  int distance = tofSensor.getDistance();  // mm
  tofSensor.clearInterrupt();
  tofSensor.stopRanging();

  Serial.print("Distance(mm): ");
  Serial.println(distance);

  delay(100);
}
