#include "FED4.h"

bool FED4::initializeVitals()
{
    bool success = true;

    // Initialize battery monitor
    if (!maxlipo.begin())
    {
        Serial.println(F("Couldn't find Adafruit MAX17048?\nMake sure a battery is plugged in!\n"));
        success = false;
    }

    // Initialize temperature/humidity sensor
    if (!aht.begin(&I2C_2))
    {
        Serial.println("Could not find AHT? Check wiring");
        success = false;
    }

    return success;
}

float FED4::getBatteryVoltage()
{
    float voltage = maxlipo.cellVoltage();
    return isnan(voltage) ? 0.0 : voltage;
}

float FED4::getBatteryPercentage()
{
    if (!isBatteryConnected())
    {
        return 0.0;
    }
    return maxlipo.cellPercent();
}

float FED4::getTemperature()
{
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    return temp.temperature;
}

float FED4::getHumidity()
{
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    return humidity.relative_humidity;
}
// does not seem to work: seeing battery voltage >> 0 with no battery connected
bool FED4::isBatteryConnected()
{
    float voltage = maxlipo.cellVoltage();
    return !isnan(voltage);
}

// optionally integrate MAX1704X flags:
// https://learn.adafruit.com/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor/arduino
// https://github.com/adafruit/Adafruit_MAX1704X/blob/main/Adafruit_MAX1704X.cpp