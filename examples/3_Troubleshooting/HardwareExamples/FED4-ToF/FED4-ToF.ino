#include <Arduino.h>
#include <Wire.h>
#include <vl53l4cd_class.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;
#define DEV_I2C Wire
#define SerialPort Serial
#define XSHUT 1

VL53L4CD sensor_vl53l4cd_sat(&DEV_I2C, A1);

void setup()
{
  SerialPort.begin(115200);
  SerialPort.println("Starting...");
   pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
    if (!mcp.begin_I2C()) {
        Serial.println("Error initializing MCP23017.");
        while (1);
    }
    mcp.pinMode(XSHUT, OUTPUT);
    mcp.digitalWrite(XSHUT, HIGH); // XSHUT must be pulled high for the sensor to be found


  DEV_I2C.begin();
  sensor_vl53l4cd_sat.begin();
  sensor_vl53l4cd_sat.VL53L4CD_Off();
  sensor_vl53l4cd_sat.InitSensor();
  sensor_vl53l4cd_sat.VL53L4CD_SetRangeTiming(200, 0);
  sensor_vl53l4cd_sat.VL53L4CD_StartRanging();
}

void loop()
{
  uint8_t NewDataReady = 0;
  VL53L4CD_Result_t results;
  uint8_t status;
  char report[64];

  do {
    status = sensor_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady);
  } while (!NewDataReady);

  if ((!status) && (NewDataReady != 0)) {
    // (Mandatory) Clear HW interrupt to restart measurements
    sensor_vl53l4cd_sat.VL53L4CD_ClearInterrupt();

    // Read measured distance. RangeStatus = 0 means valid data
    sensor_vl53l4cd_sat.VL53L4CD_GetResult(&results);
    snprintf(report, sizeof(report), "Status = %3u, Distance = %5u mm, Signal = %6u kcps/spad\r\n",
             results.range_status,
             results.distance_mm,
             results.signal_per_spad_kcps);
    SerialPort.print(report);
  }

}
