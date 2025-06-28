/*
  FED4 ToF Sensor Example
  
  This example demonstrates how to use the VL53L1X Time-of-Flight sensor
  integrated into the FED4 library.
  
  The sensor will take a distance measurement each time the device wakes up
  and output the result to the serial monitor.
  
  Hardware:
  - FED4 device with VL53L1X ToF sensor connected to I2C
  - ToF sensor XSHUT pin connected to MCP expander pin EXP_XSHUT_1
  
  Library Requirements:
  - SparkFun VL53L1X Arduino Library
  - FED4 Library
*/

#include "FED4.h"

FED4 fed4;

void setup() {
  // Initialize FED4 with program name
  if (!fed4.begin("ToF_Example")) {
    Serial.println("FED4 initialization failed!");
    while (1);
  }
  
  Serial.println("FED4 ToF Sensor Example Started");
  Serial.println("Touch any sensor to wake up and take a distance measurement");
}

void loop() {
  // Main FED4 run function handles sleep/wake cycles
  fed4.run();
  
  // When device wakes up, take a ToF measurement
  fed4.printToFDistance();
  
  // You can also get the raw distance value for custom processing:
  // int distance = fed4.readToFDistance();
  // if (distance >= 0) {
  //   Serial.printf("Custom processing: Distance = %d mm\n", distance);
  // }
  
  // Sleep for a short time before next measurement
  delay(1000);
} 