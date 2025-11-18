#include "FED4.h"

bool FED4::initializeMotion()
{
    
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
    
    // Set output data rate to 30 Hz
    if (!motionSensor.setOutputDataRate(STHS34PF80_ODR_30_HZ)) {
        return false;
    }
    
    // Set low-pass filter to ODR/50
    if (!motionSensor.setMotionLowPassFilter(STHS34PF80_LPF_ODR_DIV_50)) {
        return false;
    }
    
    // Set sensitivity (0 = medium sensitivity, lower values = more sensitive)
    // Range: -128 (ultra sensitive) to 127 (minimum sensitivity)
    if (!motionSensor.setSensitivity(-128)) {
        return false;
    }
    
    // Give sensor time to stabilize and start measuring
    delay(40);  // Sensor needs time after configuration before it can detect motion
    
    Serial.println("Motion sensor initialized!");
    return true;
}

bool FED4::motion()
{
    // First check - use isMotion() to check the motion flag
    if (!motionSensor.isMotion()) {
        return false;
    }
    // Motion detected, verify with second check
    delay(40); // Wait for new data at 30Hz (~33ms per sample)
    // Return true only if both checks detected motion
    return motionSensor.isMotion();
} 