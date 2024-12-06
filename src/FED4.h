#ifndef FED4_h
#define FED4_h

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include "Adafruit_MAX1704X.h"
#include <Stepper.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SharpMem.h>
#include <esp_adc_cal.h>
#include "esp_sleep.h"
#include "RTClib.h"
#include <SD.h>
#include <Adafruit_AHTX0.h>
#include "SD.h"
#include "FS.h"
#include <SPI.h>
#include <driver/adc.h>
#include <driver/i2s.h>
#include <driver/rtc_io.h>
#include <driver/touch_pad.h>
#include <Preferences.h>

// Pin Definitions
#include "FED4_Pins.h"

class FED4
{
public:
    // Constructor
    FED4();

    // Initialization
    void begin();

    // Core functionality (defined in FED4_Motor.cpp)
    void Feed();
    void CheckForPellet();
    void ReleaseMotor();

    // Touch sensor management (defined in FED4_Sensors.cpp)
    void CalibrateTouchSensors();
    void BaselineTouchSensors();
    void interpretTouch();
    static void IRAM_ATTR onWakeUp();
    void touch_pad_init();

    // LED control (defined in FED4_LED.cpp)
    void BluePix();
    void DimBluePix();
    void GreenPix();
    void RedPix();
    void PurplePix();
    void NoPix();
    void Vibrate(unsigned long wait);

    // Display functions (defined in FED4_Display.cpp)
    void UpdateDisplay();
    void SerialStatusReport();

    // Power management (defined in FED4_Power.cpp)
    void enterLightSleep();

    // SD card functions (defined in FED4_SD.cpp)
    bool initializeSD();
    void createDataFile();

    // Public counters and timing
    int PelletCount;
    int CenterCount;
    int LeftCount;
    int RightCount;
    int WakeCount;
    unsigned long waketime;

    // RTC functions
    void initializeRTC();
    void updateRTC();
    DateTime now();
    void serialPrintRTC();
    void adjustRTC(uint32_t timestamp);

    // Vitals functions (defined in FED4_Vitals.cpp)
    float getBatteryVoltage();
    float getBatteryPercentage();
    float getTemperature();
    float getHumidity();
    bool isBatteryConnected();

private:
    // Hardware objects
    Adafruit_MCP23X17 mcp;
    Adafruit_MAX17048 maxlipo;
    RTC_DS3231 rtc;
    Adafruit_AHTX0 aht;
    Adafruit_SharpMem display;
    Adafruit_NeoPixel pixels;
    Stepper stepper;
    TwoWire I2C_2;

    // Device state variables
    bool pelletReady;
    bool FeedReady;
    int PG1Read;
    char Event[12];
    int RetrievalTime;
    int baseline1, baseline5, baseline6;
    int threshold;
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();

    friend class FED4_Display;
    friend class FED4_Motor;
    friend class FED4_Sensors;
    friend class FED4_Power;
    friend class FED4_LED;
};

#endif