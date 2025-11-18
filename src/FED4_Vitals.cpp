#include "FED4.h"

float FED4::getBatteryVoltage()
{
    float voltage = maxlipo.cellVoltage();
    // Check for invalid readings (NaN, negative, or unreasonably high values)
    if (isnan(voltage) || voltage <= 0.0 || voltage > 5.0) {
        Serial.printf("Battery voltage invalid: %f (NaN=%d, <=0=%d, >5=%d)\n", 
                     voltage, isnan(voltage), voltage <= 0.0, voltage > 5.0);
        return 0.0; // Return 0.0 to indicate invalid reading
    }
    return voltage;
}

float FED4::getBatteryPercentage()
{
    float percent = maxlipo.cellPercent();
    if (percent > 100.0) {
        percent = 100.0;
    }

    if (percent < 0.0) {
        percent = 0.0;
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

float FED4::getWhite()
//this function reads the raw white light value from the VEML7700 sensor
{
    float whiteValue = 0.0;
    
    // Read the raw white light value from the persistent light sensor
    whiteValue = lightSensor.readWhite();
    
    // Check for invalid readings
    if (isnan(whiteValue) || whiteValue < 0) {
        return -1.0; // Return -1 to indicate invalid reading
    }
    
    return whiteValue;
}

bool FED4::initializeLightSensor()
{
    // Initialize the light sensor on the secondary I2C bus
    if (!lightSensor.begin(&I2C_2)) {
        Serial.println("Light sensor initialization failed");
        return false;
    }
    
    // Configure the sensor for optimal reading with more robust settings
    lightSensor.setGain(VEML7700_GAIN_2);  // Maximum gain for dark room sensitivity
    lightSensor.setIntegrationTime(VEML7700_IT_800MS);  // Longest integration time for maximum sensitivity
    lightSensor.powerSaveEnable(false);  // Disable power saving for maximum sensitivity
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
     while (millis() - startTime < 100) {  // 0.1 second timeout
         hum = getHumidity();
         if (hum > 5) break;  // Valid reading obtained
         delay(10);
     }
     if (hum > 5) humidity = hum;

     //get battery info with timeout
     startTime = millis();
     Serial.println("Attempting to read battery voltage and percentage...");
     while (millis() - startTime < 100) {  // 0.1 second timeout
         cellVoltage = getBatteryVoltage();
         cellPercent = getBatteryPercentage();
         Serial.printf("Battery read attempt - Voltage: %.3fV, Percent: %.1f%%\n", cellVoltage, cellPercent);
         if (cellVoltage > 0) break;  // Valid reading obtained
         delay(10);
     }
     if (cellVoltage <= 0) {
         Serial.println("Warning: Battery voltage reading failed after timeout");
     }
     if (cellPercent > 100) {
         cellPercent = 100;
     }

     //get lux with timeout
     startTime = millis();
     float luxReading = -1;
     int luxAttempts = 0;
     while (millis() - startTime < 100) {  // 0.1 second timeout
         luxReading = getLux();
         if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
         delay(10);
         luxAttempts++;
     }
     
     // If lux sensor failed after multiple attempts, try reinitializing it
     if (luxReading < 0 && luxAttempts > 10) {  // More than 500ms of failed attempts
       if (reinitializeLightSensor()) {
         // Try one more reading after reinitialization
         delay(20);  // Give sensor time to stabilize (reduced from 50ms)
         luxReading = getLux();
       }
     }
     
     if (luxReading >= 0) lux = luxReading; // Only update if we got a valid reading >= 0
     
     //get white with timeout
     startTime = millis();
     float whiteReading = -1;
     int whiteAttempts = 0;
     while (millis() - startTime < 100) {  // 0.1 second timeout
         whiteReading = getWhite();
         if (whiteReading >= 0) break;  // Valid reading obtained (white can be 0)
         delay(10);
         whiteAttempts++;
     }
     
     // If white sensor failed after multiple attempts, try reinitializing it
     if (whiteReading < 0 && whiteAttempts > 10) {  // More than 500ms of failed attempts
       if (reinitializeLightSensor()) {
         // Try one more reading after reinitialization
         delay(20);  // Give sensor time to stabilize
         whiteReading = getWhite();
       }
     }
     
     if (whiteReading >= 0) white = whiteReading; // Only update if we got a valid reading >= 0
    
}

/**
 * Polls temperature, humidity and battery sensors periodically to update their values.
 * Only updates every N minutes to avoid excessive polling.
 * Uses timeouts to prevent hanging if sensors are unresponsive.
 * @param minToUpdateSensors Number of minutes between sensor updates (default: 10)
 */
void FED4::pollSensors(int minToUpdateSensors) {
  // Increment poll counter
  pollCount++;

  // initialize motion sensor
  bool motionSensorInitialized = initializeMotion();
  if (!motionSensorInitialized) {
    Serial.println("Motion sensor initialization failed");
    return;
  }
  
  // A quirk of the ESP32-S3 is that the the primary I2C bus must
  // be exercised before I2C_2 works properly - calling prox() here does that
  // TODO: See if there is a simpler way to do this - this is a hack.
  prox();

  if (motion()) {  // check if motion is detected, keep this reading for motion percentage calculation
    motionCount++;
    motionDetected = true;  // Set flag for immediate motion detection in user programs
  }

  if (millis() - lastPollTime > (minToUpdateSensors * 60000)) {
    lastPollTime = millis();

    // Calculate motion percentage over the last period using poll count
    if (pollCount > 0) {
      motionPercentage = (float)motionCount / pollCount * 100.0;
    } else {
      motionPercentage = 0.0;
    }

    //updateDisplay();

    // Reset counters for next sampling period
    motionCount = 0;
    pollCount = 0;

    // get temp and humidity with timeouts
    unsigned long startTime = millis();
    float temp = -1;
    float hum = -1;

    //get temp with timeout
    while (millis() - startTime < 100) {  // 0.1 second timeout
      temp = getTemperature();
      if (temp > 1) break;  // Valid reading obtained
      delay(1);
    }
    if (temp > 1) temperature = temp;

    //get humidity with timeout
    startTime = millis();                 // Reset timer for humidity
    while (millis() - startTime < 100) {  // 0.1 second timeout
      hum = getHumidity();
      if (hum > 1) break;  // Valid reading obtained
      delay(1);
    }

    if (hum > 1) humidity = hum;

    //get battery info with timeout
    startTime = millis();
    while (millis() - startTime < 100) {  // 0.1 second timeout
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

    // Halt device and display warning if battery is low
    if (cellVoltage > 0 && cellVoltage < 3.5) {
      displayLowBatteryWarning();
      Serial.println("LOW BATTERY: Device halted to protect battery.");
      while (1) {
        delay(1000); // Halt here
      }
    }

    //get lux with timeout
    lightSensor.setGain(VEML7700_GAIN_2);
    lightSensor.setIntegrationTime(VEML7700_IT_800MS);
    lightSensor.enable(true);

    startTime = millis();
    float luxReading = -1;
    int luxAttempts = 0;
    while (millis() - startTime < 100) {  // 0.1 second timeout
      luxReading = getLux();
      if (luxReading >= 0) break;  // Valid reading obtained (lux can be 0)
      delay(1);
      luxAttempts++;
    }

    // If lux sensor failed after multiple attempts, try reinitializing it
    if (luxReading < 0 && luxAttempts > 2) {  // More than 2 failed attempts
      if (reinitializeLightSensor()) {
        // Try one more reading after reinitialization
        delay(1);  // Give sensor time to stabilize (reduced from 50ms)
        luxReading = getLux();
      }
    }

    if (luxReading >= 0) lux = luxReading;  // Only update if we got a valid reading >= 0

    //get white with timeout
    startTime = millis();
    float whiteReading = -1;
    int whiteAttempts = 0;
    while (millis() - startTime < 100) {  // 0.1 second timeout
      whiteReading = getWhite();
      if (whiteReading >= 0) break;  // Valid reading obtained (white can be 0)
      delay(1);
      whiteAttempts++;
    }

    // If white sensor failed after multiple attempts, try reinitializing it
    if (whiteReading < 0 && whiteAttempts > 2) {  // More than 2 failed attempts
      if (reinitializeLightSensor()) {
        // Try one more reading after reinitialization
        delay(1);  // Give sensor time to stabilize
        whiteReading = getWhite();
      }
    }

    if (whiteReading >= 0) white = whiteReading;  // Only update if we got a valid reading >= 0

    //If it's not an ActivityMonitor program, log Status to capture sensor data for each period
    if (program != "ActivityMonitor")  {
      logData("Status");
    }
        
    // Reset pollSensorsTimer so seconds display resets when data is written
    pollSensorsTimer = millis();
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
