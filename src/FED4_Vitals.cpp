#include "FED4.h"



float FED4::getBatteryVoltage()
{
    float voltage = maxlipo.cellVoltage();
    return isnan(voltage) ? 0.0 : voltage;
}

float FED4::getBatteryPercentage()
{
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

// optionally integrate MAX1704X flags:
// https://learn.adafruit.com/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor/arduino
// https://github.com/adafruit/Adafruit_MAX1704X/blob/main/Adafruit_MAX1704X.cpp

/**
 * Polls sensors at startup to get initial readings
 */
void FED4::startupPollSensors(){
    unsigned long startTime = millis();
    float temp = -1;
    float hum = -1;

     //get temp with timeout
     while (millis() - startTime < 1000) {  // 1 second timeout
         temp = getTemperature();
         if (temp > 5) break;  // Valid reading obtained
         delay(10);
     }
     if (temp > 5) temperature = temp;
    
     //get humidity with timeout
     startTime = millis();  // Reset timer for humidity
     while (millis() - startTime < 1000) {  // 1 second timeout
         hum = getHumidity();
         if (hum > 5) break;  // Valid reading obtained
         delay(10);
     }
     if (hum > 5) humidity = hum;

     //get battery info with timeout
     startTime = millis();
     while (millis() - startTime < 1000) {  // 1 second timeout
         cellVoltage = getBatteryVoltage();
         cellPercent = getBatteryPercentage();
         if (cellVoltage > 0) break;  // Valid reading obtained
         delay(10);
     }
    
     if (cellPercent > 100) {
         cellPercent = 100;
     }
}

/**
 * Polls temperature, humidity and battery sensors periodically to update their values.
 * Only updates every 10 minutes to avoid excessive polling.
 * Uses timeouts to prevent hanging if sensors are unresponsive.
 */
void FED4::pollSensors() {
  int minToUpdateSensors = 10;  //update sensors every N minutes
  if (millis()-lastPollTime > (minToUpdateSensors * 60000)) {
    lastPollTime = millis();
    // get temp and humidity with timeouts
    unsigned long startTime = millis();
    float temp = -1;
    float hum = -1;
    
    //get temp with timeout
    while (millis() - startTime < 1000) {  // 1 second timeout
      temp = getTemperature();
      if (temp > 5) break;  // Valid reading obtained
      delay(10);
    }
    if (temp > 5) temperature = temp;
    
    //get humidity with timeout
    startTime = millis();  // Reset timer for humidity
    while (millis() - startTime < 1000) {  // 1 second timeout
      hum = getHumidity();
      if (hum > 5) break;  // Valid reading obtained
      delay(10);
    }
    
    if (hum > 5) humidity = hum;

    //get battery info with timeout
    startTime = millis();
    while (millis() - startTime < 1000) {  // 1 second timeout
      cellVoltage = getBatteryVoltage();
      cellPercent = getBatteryPercentage();
      if (cellVoltage > 0) break;  // Valid reading obtained
      delay(10);
    }
    
    if (cellPercent > 100) {
      cellPercent = 100;
    }
  }
}
