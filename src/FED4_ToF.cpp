#include "FED4.h"
#include "SparkFun_VL53L1X.h"

// ToF sensor instance - using MCP pin 1 like the working script
SFEVL53L1X distanceSensor(Wire, 1); // Changed from EXP_XSHUT_1 (pin 2) to pin 1

// Helper function to test I2C connectivity
void testToFI2C() {
    Serial.println("Testing I2C connectivity for ToF sensor...");
    
    // Set I2C clock speed for better compatibility (don't reinitialize)
    Wire.setClock(100000); // Set to 100kHz for better compatibility
    
    // Scan I2C bus for devices
    Serial.println("Scanning I2C bus for devices:");
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
        }
    }
    
    // Check if VL53L1X is present (default address is 0x29)
    Wire.beginTransmission(0x29);
    byte error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("VL53L1X sensor found at address 0x29");
    } else {
        Serial.printf("VL53L1X sensor not found at address 0x29 (error: %d)\n", error);
    }
}

bool FED4::initializeToF()
{
    Serial.println("=== ToF Sensor Initialization ===");
    
    // Step 1: Configure XSHUT pin (using MCP pin 1 like the working script)
    Serial.println("Step 1: Configuring XSHUT pin...");
    mcp.pinMode(1, OUTPUT); // Use pin 1 instead of EXP_XSHUT_1 (pin 2)
    mcp.digitalWrite(1, HIGH); // XSHUT must be pulled high for the sensor to be found
    
    // Step 2: Set I2C clock speed for better compatibility
    Serial.println("Step 2: Configuring I2C clock speed...");
    Wire.setClock(100000); // Set to 100kHz for better compatibility
    delay(100);
    
    // Step 3: Test I2C connectivity
    Serial.println("Step 3: Testing I2C connectivity...");
    testToFI2C();
    
    // Step 4: Attempt to initialize the sensor (simplified like working script)
    Serial.println("Step 4: Attempting to initialize ToF sensor...");
    int result = distanceSensor.begin();
    
    if (result != 0)  // Begin returns 0 on a good init
    {
        Serial.printf("ToF sensor failed to begin. Error code: %d\n", result);
        Serial.println("Possible causes:");
        Serial.println("1. Sensor not connected to I2C bus");
        Serial.println("2. Sensor not powered (check LDO2)");
        Serial.println("3. I2C address conflict");
        Serial.println("4. XSHUT pin not properly configured");
        Serial.println("5. Sensor may need longer wake-up time");
        Serial.println("6. Sensor may be damaged or faulty");
        return false;
    }
    
    Serial.println("ToF sensor initialized successfully");
    
    // Step 5: Test basic functionality
    Serial.println("Step 5: Testing basic sensor functionality...");
    distanceSensor.startRanging();
    delay(100);
    
    if (distanceSensor.checkForDataReady()) {
        int testDistance = distanceSensor.getDistance();
        Serial.printf("Test reading successful: %d mm\n", testDistance);
        distanceSensor.clearInterrupt();
        distanceSensor.stopRanging();
        Serial.println("=== ToF Sensor Initialization Complete ===");
        return true;
    } else {
        Serial.println("Warning: Sensor not responding to ranging commands");
        distanceSensor.stopRanging();
        return false;
    }
}

int FED4::Prox()
{
    // Ensure XSHUT is high (should already be set from initialization)
    mcp.digitalWrite(1, HIGH); // Use pin 1 instead of EXP_XSHUT_1
    delay(10); // Give sensor time to wake up
    
    // Try to initialize the sensor if not already done
    if (distanceSensor.begin() != 0)  // Begin returns 0 on a good init
    {
        Serial.println("ToF sensor failed to begin.");
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