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
    void feed();
    void checkForPellet();
    void releaseMotor();

    // Touch sensor management (defined in FED4_Sensors.cpp)
    void calibrateTouchSensors();
    void baselineTouchSensors();
    void interpretTouch();
    // static void IRAM_ATTR onLeftWakeUp();
    // static void IRAM_ATTR onCenterWakeUp();
    // static void IRAM_ATTR onRightWakeUp();
    static void IRAM_ATTR onTouchWakeUp();
    void touchPadInit();

    // LED control (defined in FED4_LED.cpp)
    void bluePix();
    void dimBluePix();
    void greenPix();
    void redPix();
    void purplePix();
    void noPix();
    void vibrate();

    // Display functions (defined in FED4_Display.cpp)
    void updateDisplay();
    void serialStatusReport();

    // Power management (defined in FED4_Power.cpp)
    void enterLightSleep();

    // SD card functions (defined in FED4_SD.cpp)
    bool initializeSD();
    void createLogFile();
    void logData();

    // Public counters and timing
    int pelletCount;
    int centerCount;
    int leftCount;
    int rightCount;
    int wakeCount;
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

    void monitorTouchSensors();

    void setEvent(const String &newEvent)
    {
        event = newEvent;
    }

    String getEvent() const
    {
        return event;
    }

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
    bool feedReady;
    int pg1Read;
    String event;
    int retrievalTime;
    int touchPadLeftBaseline, touchPadCenterBaseline, touchPadRightBaseline;
    int threshold;
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();

    // Replace individual trigger flags with a single atomic type
    volatile uint8_t touchTriggers;
    static constexpr uint8_t LEFT_TRIGGER = 0x01;
    static constexpr uint8_t CENTER_TRIGGER = 0x02;
    static constexpr uint8_t RIGHT_TRIGGER = 0x04;

    uint16_t lastTouchValue; // Store the touch value that triggered the interrupt

    friend class FED4_Display;
    friend class FED4_Motor;
    friend class FED4_Sensors;
    friend class FED4_Power;
    friend class FED4_LED;
    friend class FED4_SD;
    friend class FED4_Vitals;
};

#endif