/*
Example for using the motion sensor functionality in the FED4 library

This example demonstrates how to:
1. Initialize the FED4 device with motion sensor support
2. Check for motion detection in a simple loop
3. Display motion status on the FED4 display

Hardware Requirements:
- FED4 device with STHS34PF80 motion sensor connected to I2C_2 bus (pins 20, 19)
- Motion sensor powered by LDO2 (pin 47)

Library Dependencies:
- FED4 library
- SparkFun STHS34PF80 Arduino Library
*/

#include "FED4.h"

FED4 fed4;

void setup() {
  // Initialize FED4 with program name
  if (!fed4.begin("Motion_Example")) {
    Serial.println("FED4 initialization failed!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("FED4 Motion Sensor Example Started");
  Serial.println("Move your hand near the motion sensor to test detection");
}

void loop() {
  // Check for motion
  bool motionDetected = fed4.isMotionDetected();

  // Update display with motion status
  if (motionDetected) {
    Serial.println("Motion detected!");
  } else {
    Serial.println("No motion");
  }

  // Small delay to prevent overwhelming the sensor
  delay(100);
}