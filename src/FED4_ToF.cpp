#include "FED4.h"

bool FED4::initializeToF()
{
    mcp.pinMode(XSHUT, OUTPUT);
    mcp.digitalWrite(XSHUT, HIGH); // XSHUT must be pulled high for the sensor to be found
    DEV_I2C.begin();
    sensor_vl53l4cd_sat.begin();
    sensor_vl53l4cd_sat.VL53L4CD_Off();
    sensor_vl53l4cd_sat.InitSensor();
    sensor_vl53l4cd_sat.VL53L4CD_SetRangeTiming(200, 0);
    sensor_vl53l4cd_sat.VL53L4CD_StartRanging();
    return true;
}

uint8_t FED4::readToF()
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