#include "FED4.h"

bool FED4::initializeMotion()
{
    // Test I2C connectivity 
    I2C_2.beginTransmission(0x5A); // STHS34PF80 default address
    byte error = I2C_2.endTransmission();
    
    sths34pf80_tmos_drdy_status_t dataReady;
    
    if (error != 0 ||
        !motionSensor.begin(0x5A, I2C_2) ||
        motionSensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz) != 0 ||
        motionSensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE) != 0 ||
        motionSensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_20) != 0 ||
        motionSensor.setMotionThreshold(50) != 0 ||
        motionSensor.setMotionHysteresis(50) != 0 ||
        motionSensor.getDataReady(&dataReady) != 0) {
        Serial.println("Motion sensor initialization failed");
        return false;
    }
    
    return true;
}

bool FED4::motion()
{
    sths34pf80_tmos_func_status_t status;
    
    // First check
    if (motionSensor.getStatus(&status) != 0) {
        Serial.println("Motion sensor communication error");
        return false;
    }
    
    // If no motion on first check, return false
    if (status.mot_flag != 1) {
        return false;
    }
    
    // Motion detected, verify with second check
    delay(40); // Wait for new data at 30Hz (~33ms per sample)
    if (motionSensor.getStatus(&status) != 0) {
        Serial.println("Motion sensor communication error (check 2)");
        return false;
    }
    
    // Return true only if both checks detected motion
    return (status.mot_flag == 1);
} 