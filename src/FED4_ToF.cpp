#include "FED4.h"

bool FED4::initializeToF()
{
    // Configure shutdown pins
    mcp.pinMode(EXP_XSHUT_1, OUTPUT);
    mcp.pinMode(EXP_XSHUT_2, OUTPUT);
    mcp.pinMode(EXP_XSHUT_3, OUTPUT);

    // Start with all sensors in reset
    mcp.digitalWrite(EXP_XSHUT_1, LOW);
    mcp.digitalWrite(EXP_XSHUT_2, LOW);
    mcp.digitalWrite(EXP_XSHUT_3, LOW);
    delay(10);

    // Initialize sensors with unique addresses
    setToFIDs();
    return true;
}

void FED4::setToFIDs()
{
    // Initialize first sensor
    mcp.digitalWrite(EXP_XSHUT_1, HIGH);
    delay(10);
    if (!tofSensor1.begin())
    {
        Serial.println(F("Failed to boot first VL6180X"));
        return;
    }
    tofSensor1.setAddress(LOX1_ADDRESS);
    delay(10);

    // Initialize second sensor
    mcp.digitalWrite(EXP_XSHUT_2, HIGH);
    delay(10);
    if (!tofSensor2.begin())
    {
        Serial.println(F("Failed to boot second VL6180X"));
        return;
    }
    tofSensor2.setAddress(LOX2_ADDRESS);
    delay(10);

    // Initialize third sensor
    mcp.digitalWrite(EXP_XSHUT_3, HIGH);
    delay(10);
    if (!tofSensor3.begin())
    {
        Serial.println(F("Failed to boot third VL6180X"));
        return;
    }
    tofSensor3.setAddress(LOX3_ADDRESS);
}

uint8_t FED4::readToFSensor(Adafruit_VL6180X &sensor, uint8_t *status)
{
    uint8_t range = sensor.readRange();
    uint8_t sensorStatus = sensor.readRangeStatus();

    lux = sensor.readLux(VL6180X_ALS_GAIN_5);

    if (status != nullptr)
    {
        *status = sensorStatus;
    }

    if (sensorStatus == VL6180X_ERROR_NONE)
    {
        return range;
    }
    return 0;
}

uint8_t FED4::readRightToF(uint8_t *status)
{
    return readToFSensor(tofSensor1, status);
}

uint8_t FED4::readMiddleToF(uint8_t *status)
{
    return readToFSensor(tofSensor3, status);
}

uint8_t FED4::readLeftToF(uint8_t *status)
{
    return readToFSensor(tofSensor2, status);
}
