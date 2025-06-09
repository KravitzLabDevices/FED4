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

float FED4::getLux()
//this function is a hack to get the lux value without interfering with the side neopixel 
//basically it initializes the light sensor on the secondary I2C bus, reads the lux value, and then shuts down the sensor
//as we only need to read the lux value infrequently, this is a good compromise
{
    float luxValue = 0.0;
    
    // Temporarily initialize the light sensor only for reading
    Adafruit_VEML7700 tempVeml = Adafruit_VEML7700();
    
    if (tempVeml.begin(&I2C_2)) {
        // Configure the sensor for quick reading  
        tempVeml.setGain(VEML7700_GAIN_1);
        tempVeml.setIntegrationTime(VEML7700_IT_25MS);
        
        // Give a small delay for the sensor to stabilize
        delay(50);
        
        // Read the lux value
        luxValue = tempVeml.readLux();
        
        if (isnan(luxValue)) {
            luxValue = 0.0;
        }
    } else {
        luxValue = -1.0;  // Return -1 if sensor initialization fails
    }
    
    return luxValue;
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

     //get lux with timeout
     startTime = millis();
     float luxReading = -1;
     while (millis() - startTime < 1000) {  // 1 second timeout
         luxReading = getLux();
         if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
         delay(10);
     }
     if (luxReading >= 0) lux = luxReading;
}

/**
 * Polls temperature, humidity and battery sensors periodically to update their values.
 * Only updates every 10 minutes to avoid excessive polling.
 * Uses timeouts to prevent hanging if sensors are unresponsive.
 */
void FED4::pollSensors() {
  int minToUpdateSensors = 1;  //update sensors every N minutes
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

    //get lux with timeout
    startTime = millis();
    float luxReading = -1;
    while (millis() - startTime < 1000) {  // 1 second timeout
      luxReading = getLux();
      if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
      delay(10);
    }
    if (luxReading >= 0) lux = luxReading;
  }
}
