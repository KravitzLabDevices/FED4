#include "FED4.h"
#include "SparkFun_VL53L1X.h"

// ToF sensor instance
SFEVL53L1X distanceSensor(Wire, EXP_XSHUT_1);

bool FED4::initializeToF()
{
    Serial.println("Initializing ToF sensor...");
    
    // Configure XSHUT pin on MCP expander
    mcp.pinMode(EXP_XSHUT_1, OUTPUT);
    mcp.digitalWrite(EXP_XSHUT_1, HIGH);  // XSHUT must be pulled high for the sensor to be found
    delay(10); // Give sensor time to wake up
    
    // Initialize the sensor
    if (distanceSensor.begin() != 0)  // Begin returns 0 on a good init
    {
        Serial.println("ToF sensor failed to begin.");
        return false;
    }
    
    Serial.println("ToF sensor online!");
    return true;
}

int FED4::Prox()
{
    mcp.pinMode(EXP_XSHUT_1, OUTPUT);
    mcp.digitalWrite(EXP_XSHUT_1, HIGH);  // XSHUT must be pulled high for the sensor to be found
    delay(10); // Give sensor time to wake up
    
    // Initialize the sensor
    if (distanceSensor.begin() != 0)  // Begin returns 0 on a good init
    {
        Serial.println("ToF sensor failed to begin.");
        return false;
    }

    int distance = -1; // Default error value
    
    // Start ranging
    distanceSensor.startRanging();
    
    // Wait for data to be ready
    while (!distanceSensor.checkForDataReady()) {
        delay(1);
    }
    
    // Get the distance measurement
    int calibration = 20; //set calibration value here
    distance = distanceSensor.getDistance() - calibration;
    
    // Clear interrupt and stop ranging
    distanceSensor.clearInterrupt();
    distanceSensor.stopRanging();
    
    return distance;
} 