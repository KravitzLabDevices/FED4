#include "FED4.h"
#include "SparkFun_VL53L1X.h"

// ToF sensor instance - using MCP pin 1 like the working script
SFEVL53L1X distanceSensor(Wire, 1); // Changed from EXP_XSHUT_1 (pin 2) to pin 1

bool FED4::initializeToF()
{
    // Configure XSHUT pin (using MCP pin 1 like the working script)
    mcp.pinMode(1, OUTPUT); // Use pin 1 instead of EXP_XSHUT_1 (pin 2)
    mcp.digitalWrite(1, HIGH); // XSHUT must be pulled high for the sensor to be found
    delay(10); // Give sensor more time to power up
    
    // Set I2C clock speed for better compatibility
    Wire.setClock(100000); // Set to 100kHz for better compatibility
    delay(10);
    
    // Retry logic like the working battery monitor
    int maxRetries = 3;
    int retryCount = 0;
    bool sensorInitialized = false;
    
    while (!sensorInitialized && retryCount < maxRetries) {
        retryCount++;
        Serial.printf("ToF sensor init attempt %d...\n", retryCount);
        
        // Reset I2C bus if this isn't the first attempt
        if (retryCount > 1) {
            Wire.end();
            delay(50); // Give bus time to reset
            Wire.begin(SDA, SCL);
            Wire.setClock(100000);
            delay(10);
        }
        
        // Attempt to initialize the sensor - just call begin() like prox() does
        int result = distanceSensor.begin();
        
        if (result == 0) {  // Begin returns 0 on a good init
            sensorInitialized = true;
            Serial.printf("ToF sensor initialized successfully (attempt %d)\n", retryCount);
        } else {
            Serial.printf("ToF sensor begin() failed (attempt %d) - error code: %d\n", retryCount, result);
        }
        
        // Wait before retry
        if (!sensorInitialized && retryCount < maxRetries) {
            delay(1000); // Wait 1 second before retry
        }
    }
    
    if (!sensorInitialized) {
        Serial.println("ToF sensor initialization failed after all retries");
        return false;
    }
    
    return true;
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
    
    // Wait for data to be ready with 100ms timeout
    unsigned long startTime = millis();
    while (!distanceSensor.checkForDataReady()) {
        if (millis() - startTime > 100) { // 100ms timeout
            distanceSensor.stopRanging();
            return -1; // Timeout error
        }
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