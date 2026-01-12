/*
  Reading distance from the laser based VL53L1X in FED4
  
  Based on SparkFUn libary
  By: Nathan Seidle
  SparkFun Electronics
  Date: April 4th, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun! https://www.sparkfun.com/products/14667

  This example prints the distance to an object.

  Are you getting weird readings? Be sure the vacuum tape has been removed from the sensor.
*/

#include <Wire.h>
#include "SparkFun_VL53L1X.h"  //Click here to get the library: http://librarymanager/All#SparkFun_VL53L1X
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

#define DEV_I2C Wire
#define SerialPort Serial
#define XSHUT 1

//Start ToF
SFEVL53L1X distanceSensor(Wire, XSHUT);

void setup(void) {
  SerialPort.begin(115200);
  SerialPort.println("Starting...");
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  if (!mcp.begin_I2C()) {
    Serial.println("Error initializing MCP23017.");
    while (1)
      ;
  }
  mcp.pinMode(XSHUT, OUTPUT);
  mcp.digitalWrite(XSHUT, HIGH);  // XSHUT must be pulled high for the sensor to be found
  DEV_I2C.begin();

  if (distanceSensor.begin() != 0)  //Begin returns 0 on a good init
  {
    Serial.println("Sensor failed to begin. Please check wiring. Freezing...");
    while (1)
      ;
  }
  Serial.println("Sensor online!");
}

void loop(void) {
  distanceSensor.startRanging();  //Write configuration bytes to initiate measurement
  while (!distanceSensor.checkForDataReady()) {
    delay(1);
  }
  int distance = distanceSensor.getDistance();  //Get the result of the measurement from the sensor
  distanceSensor.clearInterrupt();
  distanceSensor.stopRanging();

  Serial.print("Distance(mm): ");
  Serial.println(distance);
}
