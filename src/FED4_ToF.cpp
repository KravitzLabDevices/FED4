#include "FED4.h"
#include "SparkFun_VL53L1X.h"

// ToF sensor instance - using MCP pin 1 like the working script
SFEVL53L1X distanceSensor(Wire, 1); // Changed from EXP_XSHUT_1 (pin 2) to pin 1

bool FED4::initializeToF()
{
    // Configure XSHUT pin (using MCP pin 1 like the working script)
    mcp.pinMode(1, OUTPUT); // Use pin 1 instead of EXP_XSHUT_1 (pin 2)
    mcp.digitalWrite(1, HIGH); // XSHUT must be pulled high for the sensor to be found
    
    // Set I2C clock speed for better compatibility
    Wire.setClock(100000); // Set to 100kHz for better compatibility
    delay(10);
    
    // Attempt to initialize the sensor
    int result = distanceSensor.begin();
    
    if (result != 0)  // Begin returns 0 on a good init
    {
        Serial.println("ToF sensor failed to initialize");
        return false;
    }
    
    // Test basic functionality
    distanceSensor.startRanging();
    delay(10);
    
    if (distanceSensor.checkForDataReady()) {
        distanceSensor.clearInterrupt();
        distanceSensor.stopRanging();
        return true;
    } else {
        Serial.println("ToF sensor not responding");
        distanceSensor.stopRanging();
        return false;
    }
}

int FED4::prox()
{
    // Ensure XSHUT is high (should already be set from initialization)
    mcp.digitalWrite(1, HIGH); // Use pin 1 instead of EXP_XSHUT_1
    delay(1); // Give sensor time to wake up
    
    // Try to initialize the sensor if not already done
    if (distanceSensor.begin() != 0)  // Begin returns 0 on a good init
    {
        return -1;  // Return -1 for error instead of false
    }

    int distance = -1; // Default error value
    
    // Start ranging
    distanceSensor.startRanging();
    
    // Wait for data to be ready
    while (!distanceSensor.checkForDataReady()) {
        delay(1);
    }
    
    // Get the distance measurement
    int calibration = 20; //set calibration value here, default is 20mm 
    distance = distanceSensor.getDistance() - calibration;

    //limit reported distance to 0-150mm, with error value of -1
    if (distance >= -20 && distance < 0) {
        distance = 0;
    }
    if (distance > 150) {
        distance = 150;
    }
    //error value of -1 if sensor is not responding
    if (distance == -21) {
        distance = -1;
    }

    // Clear interrupt and stop ranging
    distanceSensor.clearInterrupt();
    distanceSensor.stopRanging();
    
    return distance;
} 