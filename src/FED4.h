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
#include <ArduinoJson.h>

// Pin Definitions
#include "FED4_Pins.h"

// Display Colors and Constants
static const uint8_t DISPLAY_BLACK = 0;
static const uint8_t DISPLAY_WHITE = 1;
static const uint8_t DISPLAY_INVERSE = 2;
static const uint8_t DISPLAY_NORMAL = 3;

// Common display dimensions
static const uint16_t DISPLAY_WIDTH = 144;
static const uint16_t DISPLAY_HEIGHT = 168;

static const uint8_t NUMPIXELS = 1;
static const uint16_t MOTOR_STEPS = 512;
static const uint8_t MOTOR_SPEED = 12;

static const float TOUCH_THRESHOLD = 0.01; // percentage of baseline change to trigger poke - note that when plugged in by USB this can be much more sensitive than on battery power, due to different grounding
static const char *META_FILE = "/meta.json";

// current verty public-oriented, consider pushing some to private
class FED4
{
public:
    // Constructor
    FED4();

    // Initialization
    void begin();
    void feed();
    bool checkForPellet();

    // Motor functionality (defined in FED4_Motor.cpp)
    void initializeMotor();
    void releaseMotor();
    void minorJamClear();
    void majorJamClear();
    void vibrateJamClear();

    // Haptic motor
    void haptic();

    // Touch sensor management (defined in FED4_Sensors.cpp)
    void initializeTouch();
    void calibrateTouchSensors();
    void interpretTouch();
    static void IRAM_ATTR onTouchWakeUp();
    void monitorTouchSensors();

    // LED control (defined in FED4_LED.cpp)
    void initializeLEDs();
    void bluePix();
    void dimBluePix();
    void greenPix();
    void redPix();
    void purplePix();
    void noPix();

    // Display functions (defined in FED4_Display.cpp)
    void initializeDisplay();
    void updateDisplay();
    void serialStatusReport();

    // Power management (defined in FED4_Power.cpp)
    void enterLightSleep();
    void initializeLDOs();
    void LDO2_ON();
    void LDO2_OFF();
    void LDO3_ON();
    void LDO3_OFF();

    // SD card functions (defined in FED4_SD.cpp)
    bool initializeSD();
    void createLogFile();
    void logData();
    String getMetaValue(const char *rootKey, const char *subKey);

    // Public counters and timing
    int pelletCount;
    int centerCount;
    int leftCount;
    int rightCount;
    int wakeCount;
    bool leftTouch;
    bool centerTouch;
    bool rightTouch;
    unsigned long waketime;

    // RTC functions
    void initializeRTC();
    void updateRTC();
    DateTime now();
    void serialPrintRTC();
    void adjustRTC(uint32_t timestamp);

    // Vitals functions (defined in FED4_Vitals.cpp)
    void initializeVitals();
    float getBatteryVoltage();
    float getBatteryPercentage();
    float getTemperature();
    float getHumidity();
    bool isBatteryConnected();

    // Speaker functions (defined in FED4_Speaker.cpp)
    void initializeSpeaker();
    void playTone(uint32_t frequency, uint32_t duration_ms);
    void resetSpeaker();
    void playStartup();

    void setEvent(const String &newEvent)
    {
        event = newEvent;
    }

    String getEvent() const
    {
        return event;
    }

    bool pelletReady;
    bool feedReady;
    int photogate1State;
    String event;
    int retrievalTime;
    int touchPadLeftBaseline;
    int touchPadCenterBaseline;
    int touchPadRightBaseline;
    int motorTurns;
    int reBaselineTouches;
    char filename[20];

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
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();

    uint16_t lastTouchValue; // Store the touch value that triggered the interrupt

    friend class FED4_Display;
    friend class FED4_LED;
    friend class FED4_Motor;
    friend class FED4_Power;
    friend class FED4_RTC;
    friend class FED4_SD;
    friend class FED4_Sensors;
    friend class FED4_Vitals;
};

#endif