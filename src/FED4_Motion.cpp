#include "FED4.h"

bool FED4::initializeMotion()
{
    // Reset flag at start of initialization
    motionSensorInitialized = false;
    
    // Test I2C connectivity 
    I2C_2.beginTransmission(0x5A); // STHS34PF80 default address
    byte error = I2C_2.endTransmission();
    
    if (error != 0) {
        Serial.print("Motion Sensor I2C FAILED");
        return false;
    }
    
    // Initialize with Adafruit library
    if (!motionSensor.begin(0x5A, &I2C_2)) {
        return false;
    }
    
    // Set low-pass filter 
    if (!motionSensor.setMotionLowPassFilter(STHS34PF80_LPF_ODR_DIV_50)) {
        return false;
    }
    
    // Set sensitivity to high but not maximum (can cause instability)
    // Range: -128 (ultra sensitive) to 127 (minimum sensitivity)
    if (!motionSensor.setSensitivity(-64)) {
        return false;
    }
    
    // // Use moderate averaging for better stability (default is 32)
    // // Using 8 for good balance between speed and stability
    // if (!motionSensor.setObjAveraging(STHS34PF80_AVG_TMOS_8)) {
    //     return false;
    // }
    
    // Set to 30Hz - maximum rate for motion detection
    // Motion detection REQUIRES continuous operation to maintain baseline
    if (!motionSensor.setOutputDataRate(STHS34PF80_ODR_30_HZ)) {
        return false;
    }

    // Verify sensor is working by reading temperature
    int16_t motionValue1 = motionSensor.readMotion();
    bool motionFlag1 = motionSensor.isMotion();
    
    Serial.println("Motion sensor initialized at 30Hz!");

    // Set flag to prevent unnecessary reinitialization
    motionSensorInitialized = true;
    
    return true;
}

bool FED4::motion()
{
    // We use the STHS34PF80 sensor to check for motion in two ways:
    // 1. By checking the internal motion flag from the sensor's algorithm
    // 2. By checking the raw motion value and comparing it to a threshold
    // if either check is true, we return true
   
    // Note: pollCount is incremented in pollSensors() before calling motion()
    
    // Get current time
    DateTime now = rtc.now();
    
    // Read motion raw  motion and internal motion flag values
    int16_t motionValue1 = motionSensor.readMotion();
    bool motionFlag1 = motionSensor.isMotion();
   
    // Print current time and motion values
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d - ", 
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
    Serial.print("Motion flag: ");
    Serial.print(motionFlag1 ? "TRUE" : "FALSE");
    Serial.print(", Motion raw: ");
    Serial.print(motionValue1);

    // Check for motion using flag OR raw value threshold
    if (motionFlag1 || abs(motionValue1) > 50) {
        // Wait for new data at 30Hz (~33ms per sample)
        delay(35);

        // Second check to confirm
        int16_t motionValue2 = motionSensor.readMotion();
        bool motionFlag2 = motionSensor.isMotion();

        // If either check is true, we return true
        if (motionFlag2 || abs(motionValue2) > 50) {
            motionCount++;
            
            // Update motion percentage after incrementing motionCount
            motionPercentage = (float)motionCount / pollCount * 100.0;
            
            Serial.print(", MOTION DETECTED! - ");
            Serial.print(motionPercentage, 2);
            Serial.print("% (");
            Serial.print(motionCount);
            Serial.print("/");
            Serial.print(pollCount);
            Serial.println(")");
            
            return true;
        }
    }
    
    // Update motion percentage even when no motion detected
    motionPercentage = (float)motionCount / pollCount * 100.0;
    
    Serial.print(" - ");
    Serial.print(motionPercentage, 2);
    Serial.print("% (");
    Serial.print(motionCount);
    Serial.print("/");
    Serial.print(pollCount);
    Serial.println(")");
    
    return false;
}

// Reset motion tracking counters (call after logging data)
void FED4::resetMotionCounters()
{
    motionCount = 0;
    pollCount = 0;
    motionPercentage = 0.0;
} 