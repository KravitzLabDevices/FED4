#include "FED4.h"

// ToF sensor instance - no XSHUT pin on v1.7 board
SFEVL53L1X distanceSensor(Wire);

bool FED4::initializeToF()
{
    // Set I2C clock speed for better compatibility
    Wire.setClock(100000); // Set to 100kHz
    delay(10);
    
    // Retry logic for reliable initialization
    int maxRetries = 3;
    int retryCount = 0;
    bool sensorInitialized = false;
    
    while (!sensorInitialized && retryCount < maxRetries) {
        retryCount++;
        Serial.println("Initializing ToF sensor");
        
        // Reset I2C bus on retry
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
    int calibration = 20; // calibration offset in mm
    distance = distanceSensor.getDistance() - calibration;

    // Limit reported distance to 0-150mm, error value of -1
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
