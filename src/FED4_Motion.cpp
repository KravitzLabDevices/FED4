#include "FED4.h"

bool FED4::initializeMotionSensor()
{
    // Try to establish communication with sensor
    uint8_t retries = 0;
    const uint8_t MAX_RETRIES = 3;

    while (!motionSensor.begin(MOTION_SENSOR_I2C_ADDRESS, I2C_2))
    {
        Serial.println("Motion sensor not found, retrying...");
        if (++retries >= MAX_RETRIES)
        {
            return false;
        }
        delay(1000);
    }

    // Configure default settings
    configureMotionSensor();
    return true;
}

void FED4::configureMotionSensor(uint16_t threshold, uint8_t hysteresis)
{
    motionSensor.setPresenceThreshold(threshold);
    motionSensor.setPresenceHysteresis(hysteresis);
}

bool FED4::isMotionDataReady()
{
    sths34pf80_tmos_drdy_status_t dataReady;
    if (!motionSensor.getDataReady(&dataReady))
    {
        return false;
    }
    return (dataReady.drdy == 1);
}

bool FED4::getMotionStatus(sths34pf80_tmos_func_status_t *status)
{
    return motionSensor.getStatus(status);
}

bool FED4::getPresenceValue(int16_t *presenceVal)
{
    return motionSensor.getPresenceValue(presenceVal);
}

bool FED4::getMotionValue(int16_t *motionVal)
{
    return motionSensor.getMotionValue(motionVal);
}

bool FED4::getTemperatureValue(float *tempVal)
{
    return motionSensor.getTemperatureData(tempVal);
}
