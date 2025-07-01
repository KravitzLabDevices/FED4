#include "FED4.h"



float FED4::getBatteryVoltage()
{
    float voltage = maxlipo.cellVoltage();
    // Check for invalid readings (NaN, negative, or unreasonably high values)
    if (isnan(voltage) || voltage <= 0.0 || voltage > 5.0) {
        return 0.0; // Return 0.0 to indicate invalid reading
    }
    return voltage;
}

float FED4::getBatteryPercentage()
{
    float percent = maxlipo.cellPercent();
    // Check for invalid readings (NaN, negative, or over 100%)
    if (isnan(percent) || percent < 0.0 || percent > 100.0) {
        return 0.0; // Return 0.0 to indicate invalid reading
    }
    return percent;
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
//this function now uses the persistent light sensor object instead of creating a temporary one
{
    float luxValue = 0.0;
    
    // Read the lux value from the persistent light sensor
    luxValue = lightSensor.readLux();
    
    // Check for invalid readings
    if (isnan(luxValue) || luxValue < 0) {
        return -1.0; // Return -1 to indicate invalid reading
    }
    
    return luxValue;
}

float FED4::getALS()
//this function reads the raw ALS value from the VEML7700 sensor
{
    float alsValue = 0.0;
    
    // Read the raw ALS value from the persistent light sensor
    alsValue = lightSensor.readALS();
    
    // Check for invalid readings
    if (isnan(alsValue) || alsValue < 0) {
        return -1.0; // Return -1 to indicate invalid reading
    }
    
    return alsValue;
}

bool FED4::initializeLightSensor()
{
    // Initialize the light sensor on the secondary I2C bus
    if (!lightSensor.begin(&I2C_2)) {
        Serial.println("Light sensor initialization failed");
        return false;
    }
    
    // Configure the sensor for optimal reading with more robust settings
    lightSensor.setGain(VEML7700_GAIN_1_8);  // Changed from GAIN_2 to GAIN_1_8 for better sensitivity
    lightSensor.setIntegrationTime(VEML7700_IT_100MS);  // Changed from 200MS to 100MS for faster response
    
    // Enable the sensor
    lightSensor.enable(true);
    
    // Add a small delay for configuration to take effect
    delay(5);
    
    return true;
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
     
     // Debug output for battery readings
     if (cellVoltage <= 0) {
         Serial.println("Warning: Battery voltage reading failed or invalid");
     } else {
         Serial.printf("Battery: %.2fV (%.1f%%)\n", cellVoltage, cellPercent);
     }
    
     if (cellPercent > 100) {
         cellPercent = 100;
     }

     //get lux with timeout
     startTime = millis();
     float luxReading = -1;
     int luxAttempts = 0;
     while (millis() - startTime < 1000) {  // 1 second timeout
         luxReading = getLux();
         if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
         delay(10);
         luxAttempts++;
     }
     
     // If lux sensor failed after multiple attempts, try reinitializing it
     if (luxReading < 0 && luxAttempts > 50) {  // More than 500ms of failed attempts
       if (reinitializeLightSensor()) {
         // Try one more reading after reinitialization
         delay(20);  // Give sensor time to stabilize (reduced from 50ms)
         luxReading = getLux();
       }
     }
     
     if (luxReading >= 0) lux = luxReading; // Only update if we got a valid reading >= 0
     
     //get ALS with timeout
     startTime = millis();
     float alsReading = -1;
     int alsAttempts = 0;
     while (millis() - startTime < 1000) {  // 1 second timeout
         alsReading = getALS();
         if (alsReading >= 0) break;  // Valid reading obtained (ALS can be 0)
         delay(1);
         alsAttempts++;
     }
     
     // If ALS sensor failed after multiple attempts, try reinitializing it
     if (alsReading < 0 && alsAttempts > 10) {  // More than 100ms of failed attempts
       if (reinitializeLightSensor()) {
         // Try one more reading after reinitialization
         delay(20);  // Give sensor time to stabilize
         alsReading = getALS();
       }
     }
     
     if (alsReading >= 0) als = alsReading; // Only update if we got a valid reading >= 0
}

/**
 * Polls temperature, humidity and battery sensors periodically to update their values.
 * Only updates every 10 minutes to avoid excessive polling.
 * Uses timeouts to prevent hanging if sensors are unresponsive.
 */
void FED4::pollSensors() {
  // Reconfigure light sensor after every I2C bus reinitialization
  delay(1);  // Brief delay for bus stabilization
  lightSensor.setGain(VEML7700_GAIN_2);  // Maximum gain for dark room sensitivity
  lightSensor.setIntegrationTime(VEML7700_IT_800MS);  // Longest integration time for maximum sensitivity
  lightSensor.powerSaveEnable(false);  // Disable power saving for maximum sensitivity
  lightSensor.enable(true);
  
  // Reconfigure motion sensor after I2C bus reinitialization
  if (motionSensor.begin(0x5A, I2C_2)) {
    // Reconfigure motion sensor settings
    motionSensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz);
    motionSensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE);
    motionSensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_20);
    motionSensor.setMotionThreshold(20);
    motionSensor.setMotionHysteresis(5);
  }
    
  //update motion detection
  prox(); // Why does this need to be here for motion to work?
  motionDetected = motion();
  if (motionDetected) {
    whitePix(100);
    motionCount++; // Aggregate motion detections
  }
  
  

  // Increment poll counter for percentage calculation
  pollCount++;

  int minToUpdateSensors = 5;  //update sensors every N minutes
  if (millis()-lastPollTime > (minToUpdateSensors * 60000)) {
    lastPollTime = millis();
    
    // Calculate motion percentage for the last 5-minute period using actual poll count
    if (pollCount > 0) {
      motionPercentage = (float)motionCount / pollCount * 100.0;
    } else {
      motionPercentage = 0.0;
    }
    
    // Reset counters for next 5-minute period
    motionCount = 0;
    pollCount = 0;
    
    // get temp and humidity with timeouts
    unsigned long startTime = millis();
    float temp = -1;
    float hum = -1;
    
    //get temp with timeout
    while (millis() - startTime < 1000) {  // 1 second timeout
      temp = getTemperature();
      if (temp > 5) break;  // Valid reading obtained
      delay(1);
    }
    if (temp > 5) temperature = temp;
    
    //get humidity with timeout
    startTime = millis();  // Reset timer for humidity
    while (millis() - startTime < 1000) {  // 1 second timeout
      hum = getHumidity();
      if (hum > 5) break;  // Valid reading obtained
      delay(1);
    }
    
    if (hum > 5) humidity = hum;

    //get battery info with timeout
    startTime = millis();
    while (millis() - startTime < 1000) {  // 1 second timeout
      cellVoltage = getBatteryVoltage();
      cellPercent = getBatteryPercentage();
      if (cellVoltage > 0) break;  // Valid reading obtained
      delay(1);
    }
    
    // Debug output for battery readings
    if (cellVoltage <= 0) {
      Serial.println("Warning: Battery voltage reading failed or invalid during periodic poll");
    }
    
    if (cellPercent > 100) {
      cellPercent = 100;
    }

    //get lux with timeout
    lightSensor.setGain(VEML7700_GAIN_2);
    lightSensor.setIntegrationTime(VEML7700_IT_100MS);
    lightSensor.enable(true);
    
    startTime = millis();
    float luxReading = -1;
    int luxAttempts = 0;
    while (millis() - startTime < 1000) {  // 1 second timeout
      luxReading = getLux();
      if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
      delay(1);
      luxAttempts++;
    }
    
    // If lux sensor failed after multiple attempts, try reinitializing it
    if (luxReading < 0 && luxAttempts > 10) {  // More than 50ms of failed attempts
      if (reinitializeLightSensor()) {
        // Try one more reading after reinitialization
        delay(5);  // Give sensor time to stabilize (reduced from 50ms)
        luxReading = getLux();
      }
    }
    
    if (luxReading >= 0) lux = luxReading; // Only update if we got a valid reading >= 0

    //get ALS with timeout
    startTime = millis();
    float alsReading = -1;
    int alsAttempts = 0;
    while (millis() - startTime < 1000) {  // 1 second timeout
      alsReading = getALS();
      if (alsReading >= 0) break;  // Valid reading obtained (ALS can be 0)
      delay(1);
      alsAttempts++;
    }
    
    // If ALS sensor failed after multiple attempts, try reinitializing it
    if (alsReading < 0 && alsAttempts > 50) {  // More than 50ms of failed attempts
      if (reinitializeLightSensor()) {
        // Try one more reading after reinitialization
        delay(5);  // Give sensor time to stabilize
        alsReading = getALS();
      }
    }
    
    if (alsReading >= 0) als = alsReading; // Only update if we got a valid reading >= 0

    //log sensor data
    logData("StatusReport");
  }
}

/**
 * Prints current memory status for debugging memory leaks
 */
void FED4::printMemoryStatus() {
    Serial.printf("Memory Status - Free: %d, Size: %d, MinFree: %d, Fragmentation: %.1f%%\n",
                  ESP.getFreeHeap(),
                  ESP.getHeapSize(),
                  ESP.getMinFreeHeap(),
                  (float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize() * 100.0);
    
    // Additional memory info for debugging small leaks
    Serial.printf("Memory Details - Largest Block: %d, Allocated: %d, Fragments: %d\n",
                  ESP.getMaxAllocHeap(),
                  ESP.getHeapSize() - ESP.getFreeHeap(),
                  ESP.getHeapSize() - ESP.getFreeHeap() - ESP.getMaxAllocHeap());
}

/**
 * Attempts to reinitialize the light sensor if it's not responding
 */
bool FED4::reinitializeLightSensor() {
    // First, try to reset the I2C bus
    I2C_2.end();
    delay(1);  // Give bus time to reset (reduced from 50ms)
    I2C_2.begin(SDA_2, SCL_2);
    delay(1);  // Give bus time to stabilize (reduced from 50ms)
    
    // Try to initialize the sensor
    if (initializeLightSensor()) {
        // Test multiple readings to ensure stability
        for (int i = 0; i < 3; i++) {
            float testReading = lightSensor.readLux();
            if (testReading < 0) {
                return false;
            }
            delay(1);
        }
        
        return true;
    } else {
        return false;
    }
}
