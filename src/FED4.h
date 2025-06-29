#ifndef FED4_h
#define FED4_h

#include <Arduino.h>
#include <map>
#include <string>
#include <Adafruit_MCP23X17.h>
#include "Adafruit_MAX1704X.h"
#include <Stepper.h>
#include <Adafruit_NeoPixel.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Org_01.h>
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
#include "Adafruit_MLX90393.h"
#include "SparkFun_VL53L1X.h"
#include "SparkFun_STHS34PF80_Arduino_Library.h"
#include "Adafruit_VEML7700.h"
#include <ESP32Time.h>

// Pin Definitions
#include "FED4_Pins.h"

// Device Constants
static const uint8_t LIS3DH_I2C_ADDRESS = 0x19;        // Default I2C address for LIS3DH accelerometer
static const uint8_t MLX90393_I2C_ADDRESS = 0x0C;      // Default I2C address for MLX90393 magnetometer

// Display Colors and Constants
static const uint8_t DISPLAY_BLACK = 0;
static const uint8_t DISPLAY_WHITE = 1;
static const uint8_t DISPLAY_INVERSE = 2;
static const uint8_t DISPLAY_NORMAL = 3;

// Common display dimensions
static const uint16_t DISPLAY_WIDTH = 144;
static const uint16_t DISPLAY_HEIGHT = 168;

static const uint8_t NUM_STRIP_LEDS = 8;
static const uint8_t NUMPIXELS = 1;
static const uint16_t MOTOR_STEPS = 512;
static const uint8_t MOTOR_SPEED = 24;

static const float TOUCH_THRESHOLD = 0.1; // percentage of baseline change to trigger poke - note that when plugged in by USB this can be much more sensitive than on battery power, due to different grounding
static const char *META_FILE = "/meta.json";

static const char *PREFS_NAMESPACE = "fed4";
static const bool PREFS_RO_MODE = true;
static const bool PREFS_RW_MODE = false;

// current very public-oriented, consider pushing some to private
class FED4 : public Adafruit_GFX
{
public:
    // Constructor declaration only
    FED4();
    static const char libraryVer[];

    // Initialization
    bool begin(const char* programName = nullptr);
    
    // Button functions
    bool initializeButtons();
    static void IRAM_ATTR onButton1WakeUp();
    static void IRAM_ATTR onButton2WakeUp();
    static void IRAM_ATTR onButton3WakeUp();

    // Corefunctions
    void feed();
    void run();

    // Menu functions
    void menu();
    void startMenu();
    void menuProgram();
    void menuMouseId();
    void menuSex();
    void menuStrain();
    void menuAge();

    // Sensor polling
    void pollSensors();
    void startupPollSensors();

    // Pellet functions
    bool checkForPellet();
    bool didPelletDrop();
    bool pelletPresent;
    bool pelletDropped;
    void initFeeding();
    void handlePelletSettling();
    void handlePelletInWell();
    void finishFeeding();
    void dispense();
    unsigned long pelletDropTime;
    unsigned long pelletWellTime;
    bool dispenseError = false;
    void handleJams();

    // TRSS input/output connector functions
    bool initializeTRSS();
    void outputPulse(uint8_t trss, uint8_t duration);

    // Clock variables
    int currentHour;
    int currentMinute;
    int currentSecond;
    unsigned long unixtime;

    // Stepper motor functionality (defined in FED4_Motor.cpp)
    bool initializeMotor();
    void releaseMotor();
    void minorJamClear();
    void majorJamClear();
    void vibrateJamClear();
    void jammed();

    // Haptic motor vibration stimuli
    void hapticBuzz(uint8_t duration = 200);
    void hapticDoubleBuzz(uint8_t duration = 200);
    void hapticTripleBuzz(uint8_t duration = 200);

    // Touch sensor management (defined in FED4_Sensors.cpp)
    bool initializeTouch();
    void calibrateTouchSensors();
    void interpretTouch();
    static void IRAM_ATTR onTouchWakeUp();
    void monitorTouchSensors();
    void clearTouch();
    void resetTouchFlags();  // Reset all touch flags to false
    static uint8_t wakePad;  // 0=none, 1=left, 2=center, 3=right

    // Pixel an Strip control (defined in FED4_LEDs.cpp)
    // (strip)
    bool initializeStrip();
    void setStripBrightness(uint8_t brightness);
    void colorWipe(const char* colorName, unsigned long wait);
    void colorWipe(uint32_t color, unsigned long wait);
    void stripTheaterChase(const char* colorName, unsigned long wait, unsigned int groupSize = 3, unsigned int numChases = 10);
    void stripTheaterChase(uint32_t color, unsigned long wait, unsigned int groupSize = 3, unsigned int numChases = 10);
    void stripRainbow(unsigned long wait, unsigned int numLoops);
    void clearStrip();
    void setStripPixel(uint8_t pixel, uint32_t color);
    void leftLight(uint32_t color);
    void centerLight(uint32_t color);
    void rightLight(uint32_t color);
    void setStripPixel(uint8_t pixel, const char *colorName);
    void leftLight(const char *colorName);
    void centerLight(const char *colorName);
    void rightLight(const char *colorName);
    // (pixel)
    bool initializePixel();
    void setPixBrightness(uint8_t brightness);
    void setPixColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
    void setPixColor(const char *colorName, uint8_t brightness = 64);
    void bluePix(uint8_t brightness = 255);
    void greenPix(uint8_t brightness = 255);
    void redPix(uint8_t brightness = 255);
    void purplePix(uint8_t brightness = 255);
    void yellowPix(uint8_t brightness = 255);
    void cyanPix(uint8_t brightness = 255);
    void whitePix(uint8_t brightness = 255);
    void orangePix(uint8_t brightness = 255);
    void noPix();
    // (shared)
    uint32_t getColorFromString(const char *colorName);

    // Display functions (defined in FED4_Display.cpp)
    bool initializeDisplay();
    void updateDisplay();
    void displayTask();
    void displayMouseId();
    void displayStrain();
    void displaySex();
    void displayAge();
    void displayCounters();
    void displayDateTime();
    void displayEnvironmental();
    void displayBattery();
    void displayIndicators();
    void startupAnimation();

    
    void serialStatusReport();

    // Power management (defined in FED4_Power.cpp)
    void sleep();
    void startSleep();
    void wakeUp();
    void handleTouch();
    void checkButton1();
    void checkButton2();
    void checkButton3();
    bool initializeLDOs();
    void LDO2_ON();
    void LDO2_OFF();
    void LDO3_ON();
    void LDO3_OFF();

    // SD card functions (defined in FED4_SD.cpp)
    bool initializeSD();
    bool createMetaJson();
    void createLogFile();
    void logData(const String &newEvent = "");
    String getMetaValue(const char *rootKey, const char *subKey);
    bool setMetaValue(const char *rootKey, const char *subKey, const char *value);
    void setProgram(String program);
    void setMouseId(String mouseId);
    void setSex(String sex);        
    void setStrain(String strain);
    void setAge(String age);

    // Public counters and timing
    int pelletCount;
    int centerCount;
    int leftCount;
    int rightCount;
    int wakeCount = 0;
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
    float getBatteryVoltage();
    float getBatteryPercentage();
    float getTemperature();
    float getHumidity();
    float getLux();
    bool initializeLightSensor();
    bool reinitializeLightSensor();

    // variables to store temp/humidity and battery info so we don't have to keep pinging the chips every time
    float temperature;
    float humidity;
    float lux;
    float cellVoltage;
    float cellPercent;
    unsigned long lastPollTime = 0; // make this a large negative so FED polls sensors at first startup

    // Speaker functions (defined in FED4_Speaker.cpp)
    bool initializeSpeaker();
    struct Tone
    {
        uint32_t frequency;
        uint32_t duration_ms;
        float amplitude = 0.25;
    };
    void playTone(uint32_t frequency = 500, uint32_t duration_ms = 200, float amplitude = 0.25);
    void playTones(const Tone *tones, size_t count);
    void enableAmp(bool enable);
    void resetSpeaker();

    // Sound stimuli
    void playStartup();
    void bopBeep();
    void resetJingle();
    void menuJingle();
    void lowBeep();
    void highBeep();
    void higherBeep();
    void click();
    void soundSweep(uint32_t startFreq = 500, uint32_t endFreq = 1500, uint32_t duration_ms = 1000);
    void noise(uint32_t duration_ms = 500, float amplitude = 1);

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
    float retrievalTime;
    int touchPadLeftBaseline;
    int touchPadCenterBaseline;
    int touchPadRightBaseline;
    int motorTurns;
    int reBaselineTouches;
    char filename[32];

    void clearDisplay();
    void refresh();
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t size = 1);
    void drawText(const char *text, uint8_t size = 1);

    // Accelerometer functions (defined in FED4_Accel.cpp)
    bool initializeAccel();
    void setAccelRange(lis3dh_range_t range);
    void setAccelPerformanceMode(lis3dh_mode_t mode);
    void setAccelDataRate(lis3dh_dataRate_t dataRate);
    bool getAccelEvent(sensors_event_t *event);
    void readAccel(float &x, float &y, float &z);
    bool accelDataReady();

    // Magnet functions (defined in FED4_Magnet.cpp)
    bool initializeMagnet();
    void setMagnetGain(mlx90393_gain_t gain);
    mlx90393_gain_t getMagnetGain();
    bool readMagnetData(float &x, float &y, float &z);
    bool getMagnetEvent(sensors_event_t *event);
    void configureMagnet(mlx90393_gain_t gain = MLX90393_GAIN_5X);

    // ToF sensor functions (defined in FED4_ToF.cpp)
    bool initializeToF();
    int Prox();

    // Motion sensor functions (defined in FED4_Motion.cpp)
    bool initializeMotion();
    bool isMotionDetected();

    // Memory monitoring function
    void printMemoryStatus();
    
    // Debug function for lux sensor
    void debugLuxSensor();
    void testI2CConnectivity();
    void checkLightSensorConfig();

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
    ESP32Time Inrtc;
    Adafruit_AHTX0 aht;
    Adafruit_NeoPixel pixels;
    Stepper stepper;
    TwoWire I2C_2;
    CRGB strip_leds[NUM_STRIP_LEDS];
    Adafruit_LIS3DH accel;
    Adafruit_MLX90393 magnet;
    STHS34PF80_I2C motionSensor;
    Adafruit_VEML7700 lightSensor;
    // Device state variables
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;
    String program;
    String mouseId;
    String sex;
    String strain;
    String age;

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();
    void updateTime();

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
    friend class FED4_Feed;
    friend class FED4_Begin;
    friend class FED4_Speaker;
    friend class FED4_Magnet;

    friend class FED4_Accel;
    friend class FED4_Menu;
    friend class FED4_Motion;
};

// Standard ASCII 5x7 font
static const unsigned char font[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, // space
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    // ... rest of font data ...
};

#endif