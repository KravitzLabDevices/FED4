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
    
    // Set low-pass filter to ODR/20 (less filtering = more responsive)
    // Options: ODR/9 (most responsive), ODR/20, ODR/50, ODR/100, etc.
    if (!motionSensor.setMotionLowPassFilter(STHS34PF80_LPF_ODR_DIV_20)) {
        return false;
    }
    
    // Set sensitivity to ultra high (lower = more sensitive)
    // Range: -128 (ultra sensitive) to 127 (minimum sensitivity)
    if (!motionSensor.setSensitivity(-128)) {
        return false;
    }
    
    // Set hysteresis to very low (reduces "dead zone" at threshold)
    // Lower hysteresis = more responsive but potentially more flickering
    // Write directly to HYST_MOTION register (0x26) in embedded function page
    motionSensor.enableEmbeddedFuncPage(true);
    uint8_t hysteresis = 10;  // Very low hysteresis (0-255 range)
    motionSensor.writeEmbeddedFunction(0x26, &hysteresis, 1);
    motionSensor.enableEmbeddedFuncPage(false);
    delay(10);  // Brief delay after hysteresis setting
    
    // Give sensor time to stabilize and start measuring
    delay(100);  // Increased stabilization time
    
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
    delay(50);
    // Return true only if both checks detected motion
    return motionSensor.isMotion();
} 