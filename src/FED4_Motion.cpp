#include "FED4.h"

bool FED4::initializeMotion()
{
    Serial.println("=== Motion Sensor Initialization ===");
    
    // Step 1: Ensure LDO2 is enabled (should already be done in begin())
    Serial.println("Step 1: Checking power to motion sensor...");
    if (digitalRead(LDO2_ENABLE) != HIGH) {
        Serial.println("Warning: LDO2 not enabled, enabling now...");
        digitalWrite(LDO2_ENABLE, HIGH);
        delay(100); // Give sensor time to power up
    }
    
    // Step 2: Test I2C connectivity
    Serial.println("Step 2: Testing I2C connectivity...");
    I2C_2.beginTransmission(0x5A); // STHS34PF80 default address
    byte error = I2C_2.endTransmission();
    if (error != 0) {
        Serial.printf("Motion sensor not found at address 0x5A (error: %d)\n", error);
        Serial.println("Possible causes:");
        Serial.println("1. Sensor not connected to I2C_2 bus");
        Serial.println("2. Sensor not powered (check LDO2)");
        Serial.println("3. Wrong I2C address");
        Serial.println("4. Sensor may need longer wake-up time");
        return false;
    }
    Serial.println("Motion sensor found at address 0x5A");
    
    // Step 3: Initialize the sensor
    Serial.println("Step 3: Initializing motion sensor...");
    if (!motionSensor.begin(0x5A, I2C_2)) {
        Serial.println("Failed to initialize motion sensor");
        return false;
    }
    Serial.println("Motion sensor communication established");
    
    // Step 4: Configure sensor settings
    Serial.println("Step 4: Configuring sensor settings...");
    
    // Set data rate for motion sensing (30Hz)
    if (motionSensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz) != 0) {
        Serial.println("Failed to set data rate");
        return false;
    }
    
    // Set gain mode to default
    if (motionSensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE) != 0) {
        Serial.println("Failed to set gain mode");
        return false;
    }
    
    // Set low-pass filter bandwidth
    if (motionSensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_20) != 0) {
        Serial.println("Failed to set LPF bandwidth");
        return false;
    }
    
    // Set motion threshold (200 is a good starting value)
    if (motionSensor.setMotionThreshold(200) != 0) {
        Serial.println("Failed to set motion threshold");
        return false;
    }
    
    // Set motion hysteresis (10 is a good starting value)
    if (motionSensor.setMotionHysteresis(10) != 0) {
        Serial.println("Failed to set motion hysteresis");
        return false;
    }
    
    Serial.println("Motion sensor configured successfully");
    
    // Step 5: Test basic functionality
    Serial.println("Step 5: Testing basic sensor functionality...");
    delay(100); // Give sensor time to settle
    
    sths34pf80_tmos_drdy_status_t dataReady;
    if (motionSensor.getDataReady(&dataReady) == 0) {
        Serial.println("Motion sensor responding to commands");
        Serial.println("=== Motion Sensor Initialization Complete ===");
        return true;
    } else {
        Serial.println("Warning: Motion sensor not responding to commands");
        return false;
    }
}

bool FED4::Motion()
{
    // Check if sensor has new data
    sths34pf80_tmos_drdy_status_t dataReady;
    if (motionSensor.getDataReady(&dataReady) != 0) {
        return false; // Error reading data ready status
    }
    
    // If no new data, return false
    if (dataReady.drdy != 1) {
        return false;
    }
    
    // Get motion status
    sths34pf80_tmos_func_status_t status;
    if (motionSensor.getStatus(&status) != 0) {
        return false; // Error reading status
    }
    
    // Return true if motion is detected
    return (status.mot_flag == 1);
} 