#include "FED4.h"

bool FED4::initializeMotion()
{
    // Ensure LDO2 is enabled (should already be done in begin())
    if (digitalRead(LDO2_ENABLE) != HIGH) {
        digitalWrite(LDO2_ENABLE, HIGH);
        delay(100); // Give sensor time to power up
    }
    
    // Test I2C connectivity
    I2C_2.beginTransmission(0x5A); // STHS34PF80 default address
    byte error = I2C_2.endTransmission();
    if (error != 0) {
        Serial.println("Motion sensor not found");
        return false;
    }
    
    // Initialize the sensor
    if (!motionSensor.begin(0x5A, I2C_2)) {
        Serial.println("Motion sensor failed to initialize");
        return false;
    }
    
    // Configure sensor settings
    // Set data rate for motion sensing (30Hz)
    if (motionSensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz) != 0) {
        Serial.println("Motion sensor configuration failed");
        return false;
    }
    
    // Set gain mode to default
    if (motionSensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE) != 0) {
        Serial.println("Motion sensor configuration failed");
        return false;
    }
    
    // Set low-pass filter bandwidth
    if (motionSensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_9) != 0) {
        Serial.println("Motion sensor configuration failed");
        return false;
    }
    
    // Set motion threshold (200 is a good starting value)
    if (motionSensor.setMotionThreshold(20) != 0) {
        Serial.println("Motion sensor configuration failed");
        return false;
    }
    
    // Set motion hysteresis (10 is a good starting value)
    if (motionSensor.setMotionHysteresis(5) != 0) {
        Serial.println("Motion sensor configuration failed");
        return false;
    }
    
    // Test basic functionality
    delay(100); // Give sensor time to settle
    
    sths34pf80_tmos_drdy_status_t dataReady;
    if (motionSensor.getDataReady(&dataReady) == 0) {
        return true;
    } else {
        Serial.println("Motion sensor not responding");
        return false;
    }
}

bool FED4::motion()
{
    // Check if sensor has new data with 100ms timeout
    sths34pf80_tmos_drdy_status_t dataReady;
    unsigned long startTime = millis();
    
    // Try to get data ready status with timeout
    while (motionSensor.getDataReady(&dataReady) != 0) {
        if (millis() - startTime > 100) { // 100ms timeout
            return false; // Timeout error
        }
        delay(1);
    }
    
    // If no new data, return false
    if (dataReady.drdy != 1) {
        return false;
    }
    
    // Get motion status with timeout
    sths34pf80_tmos_func_status_t status;
    startTime = millis(); // Reset timeout for status check
    
    while (motionSensor.getStatus(&status) != 0) {
        if (millis() - startTime > 100) { // 100ms timeout
            return false; // Timeout error
        }
        delay(1);
    }
    
    // Return true if motion is detected
    return (status.mot_flag == 1);
} 