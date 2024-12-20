#ifndef FED4_h
#define FED4_h

#include <Arduino.h>
#include <map>
#include <string>
#include <Adafruit_MCP23X17.h>
#include "Adafruit_MAX1704X.h"
#include <Stepper.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
// #include <Adafruit_SharpMem.h>
#include <esp_adc_cal.h>
#include "esp_sleep.h"
#include "RTClib.h"
#include <SD.h>
#include "FS.h"
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <driver/adc.h>
#include <driver/i2s.h>
#include <driver/rtc_io.h>
#include <driver/touch_pad.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

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

static const char *PREFS_NAMESPACE = "fed4";
static const bool PREFS_RO_MODE = true;
static const bool PREFS_RW_MODE = false;

// current verty public-oriented, consider pushing some to private
class FED4 : public Adafruit_GFX
{
public:
    // Constructor declaration only
    FED4();

    // Initialization
    bool begin();
    void feed();
    bool checkForPellet();

    // Motor functionality (defined in FED4_Motor.cpp)
    bool initializeMotor();
    void releaseMotor();
    void minorJamClear();
    void majorJamClear();
    void vibrateJamClear();

    // Haptic motor
    void haptic();

    // Touch sensor management (defined in FED4_Sensors.cpp)
    bool initializeTouch();
    void calibrateTouchSensors();
    void interpretTouch();
    static void IRAM_ATTR onTouchWakeUp();
    void monitorTouchSensors();

    // LED control (defined in FED4_LED.cpp)
    bool initializePixel();
    void setPixBrightness(uint8_t brightness);
    void setPixColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
    void bluePix(uint8_t brightness = 255);
    void greenPix(uint8_t brightness = 255);
    void redPix(uint8_t brightness = 255);
    void purplePix(uint8_t brightness = 255);
    void yellowPix(uint8_t brightness = 255);
    void cyanPix(uint8_t brightness = 255);
    void whitePix(uint8_t brightness = 255);
    void orangePix(uint8_t brightness = 255);
    void noPix();

    // Display functions (defined in FED4_Display.cpp)
    bool initializeDisplay();
    void updateDisplay();
    void serialStatusReport();

    // Power management (defined in FED4_Power.cpp)
    void sleep();
    bool initializeLDOs();
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
    bool initializeRTC();
    void updateRTC();
    DateTime now();
    void serialPrintRTC();
    void adjustRTC(uint32_t timestamp);

    // Vitals functions (defined in FED4_Vitals.cpp)
    bool initializeVitals();
    float getBatteryVoltage();
    float getBatteryPercentage();
    float getTemperature();
    float getHumidity();
    bool isBatteryConnected();

    // Speaker functions (defined in FED4_Speaker.cpp)
    bool initializeSpeaker();
    struct Tone
    {
        uint32_t frequency;
        uint32_t duration_ms;
    };
    void playTone(uint32_t frequency, uint32_t duration_ms, bool controlAmp);
    void playTones(const Tone *tones, size_t count);
    void enableAmp(bool enable);
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
    String event = "";
    int retrievalTime;
    int touchPadLeftBaseline;
    int touchPadCenterBaseline;
    int touchPadRightBaseline;
    int motorTurns;
    int reBaselineTouches;
    char filename[20];

    void clearDisplay();
    void refresh();
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t size = 1);
    void drawText(const char *text, uint8_t size = 1);

    // LED Strip control (defined in FED4_Strip.cpp)
    bool initializeStrip();
    void setStripBrightness(uint8_t brightness);
    void colorWipe(uint32_t color, unsigned long wait);
    void stripTheaterChase(uint32_t color, unsigned long wait, unsigned int groupSize, unsigned int numChases);
    void stripRainbow(unsigned long wait, unsigned int numLoops);
    void clearStrip();
    void setStripPixel(uint8_t pixel, uint32_t color);

    // Accelerometer functions (defined in FED4_Accel.cpp)
    bool initializeAccel();
    void setAccelRange(lis3dh_range_t range);
    void setAccelPerformanceMode(lis3dh_mode_t mode);
    void setAccelDataRate(lis3dh_dataRate_t dataRate);
    bool getAccelEvent(sensors_event_t *event);
    void readAccel(float &x, float &y, float &z);
    bool accelDataReady();

    ~FED4()
    {
        if (displayBuffer)
        {
            free(displayBuffer);
            displayBuffer = nullptr;
        }
        preferences.end(); // Ensure preferences is closed
    }

private:
    // Hardware objects
    Adafruit_MCP23X17 mcp;
    Adafruit_MAX17048 maxlipo;
    RTC_DS3231 rtc;
    Adafruit_AHTX0 aht;
    Adafruit_NeoPixel pixels;
    Stepper stepper;
    TwoWire I2C_2;
    Adafruit_NeoPixel strip;
    Adafruit_LIS3DH accel;

    // Device state variables
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();

    uint16_t lastTouchValue; // Store the touch value that triggered the interrupt

    uint8_t *displayBuffer = nullptr;
    bool vcom;
    void sendDisplayCommand(uint8_t cmd);

    friend class FED4_Display;
    friend class FED4_LED;
    friend class FED4_Motor;
    friend class FED4_Power;
    friend class FED4_RTC;
    friend class FED4_SD;
    friend class FED4_Sensors;
    friend class FED4_Vitals;
};

// Standard ASCII 5x7 font
static const unsigned char font[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, // space
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    // ... rest of font data ...
};

#endif