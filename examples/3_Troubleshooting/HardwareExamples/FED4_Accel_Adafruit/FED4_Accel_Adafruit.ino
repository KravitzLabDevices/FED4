#include <Wire.h>
#include "SparkFun_LIS2DH12.h"

#define SDA_PIN 8
#define SCL_PIN 9

SPARKFUN_LIS2DH12 accel;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("LIS2DH12 Test on Custom I2C Pins");

  // Configure I2C to use custom pins
  Wire.begin(SDA_PIN, SCL_PIN);

  if (accel.begin() != 0) {
    Serial.println("Problem starting LIS2DH12 on custom pins.");
    while (1);
  }
}

void loop() {
  // Wait for new data
  while (accel.available() == false) {
    delay(1);
  }

  // Read acceleration values
  float accelX = accel.getX();
  float accelY = accel.getY();
  float accelZ = accel.getZ();

  Serial.print("X: ");
  Serial.print(accelX, 3);
  Serial.print(" g, Y: ");
  Serial.print(accelY, 3);
  Serial.print(" g, Z: ");
  Serial.print(accelZ, 3);
  Serial.println(" g");

  delay(500);
}
