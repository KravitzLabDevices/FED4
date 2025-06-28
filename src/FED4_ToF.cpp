#include "FED4.h"
#include <vl53l4cd_class.h>

// Global flag to track if ToF sensor is working
static bool tofSensorWorking = false;

bool FED4::initializeToF()
{
    Serial.println("Starting ToF sensor initialization...");
    
    // Configure XSHUT pin on MCP expander
    mcp.pinMode(EXP_XSHUT_1, OUTPUT);
    mcp.digitalWrite(EXP_XSHUT_1, HIGH); // XSHUT must be pulled high for the sensor to be found
    
    // Initialize I2C
    Wire.begin();
    
    // Initialize the sensor
    if (tofSensor.begin() != 0) {
        Serial.println("ToF sensor begin() failed");
        return false;
    }
    
    // Configure the sensor
    tofSensor.VL53L4CD_Off();
    tofSensor.InitSensor();
    tofSensor.VL53L4CD_SetRangeTiming(200, 0);
    tofSensor.VL53L4CD_StartRanging();
    
    Serial.println("ToF sensor initialized successfully");
    return true;
}

uint16_t FED4::readProx()
{
    uint8_t NewDataReady = 0;
    VL53L4CD_Result_t results;
    uint8_t status;
    
    // Check if new data is ready
    do {
        status = tofSensor.VL53L4CD_CheckForDataReady(&NewDataReady);
    } while (!NewDataReady);
    
    if ((!status) && (NewDataReady != 0)) {
        // Clear HW interrupt to restart measurements
        tofSensor.VL53L4CD_ClearInterrupt();
        
        // Read measured distance
        tofSensor.VL53L4CD_GetResult(&results);
        
        // Return distance in mm if valid data (RangeStatus = 0)
        if (results.range_status == 0) {
            return results.distance_mm;
        } else {
            return 0xFFFF; // Return max value to indicate invalid reading
        }
    }
    
    return 0xFFFF; // Return max value if no data available
}

// Function to handle ToF sensor during sleep/wake cycles
void FED4::tofSleep()
{
    // Stop ranging before sleep
    tofSensor.VL53L4CD_StopRanging();
}

void FED4::tofWake()
{
    // Restart ranging after wake
    tofSensor.VL53L4CD_StartRanging();
    delay(50); // Give sensor time to start ranging
} 