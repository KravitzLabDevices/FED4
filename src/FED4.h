#ifndef FED4_h
#define FED4_h

#include <Arduino.h>
#include <WString.h>
#include <map>
#include <string>
#include <Adafruit_MCP23X17.h> // version 2.3.2
#include "Adafruit_MAX1704X.h" // version 1.0.3
#include <Stepper.h>           // version 1.1.3
#include <FastLED.h>           // version 3.10.2
#include <Wire.h>
#include <Adafruit_GFX.h> // version 1.12.3
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/Org_01.h>
#include <esp_adc_cal.h>
#include "esp_sleep.h"
class DateTime;
#include "RTClib.h" //Adafruit version, 2.1.4
#include <SD.h>     //ESP32 version
#include "FS.h"
#include <Adafruit_BME680.h> //version 2.0.5
#include <SPI.h>
#include <driver/adc.h>
#include <ESP_I2S.h> // New I2S API for ESP32 core 3.x
#include <driver/rtc_io.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_LIS3DH.h> //version 1.3.0
#include <Adafruit_Sensor.h>
#include "SparkFun_VL53L1X.h"  //version 1.2.12
#include "Adafruit_VEML7700.h" //version 2.1.6
#include <ESP32Time.h>         //version 2.0.6

// Optional Hublink integration - can be excluded via compiler directive
#ifndef FED4_EXCLUDE_HUBLINK
#include <Hublink.h>
#endif

// Pin Definitions
#include "FED4_Pins.h"
#include "FED4_DisplayOrient.h"
#include "FED4_TouchHelpers.h"

// Sense TRRS TRIG+UART master (FED4_Submodule*) — TRRS2=TRIG, TRRS3=DATA.
// Set to 1 here (library rebuild) to expose FED4::sense*.
#ifndef FED4_ENABLE_SUBMODULE
#define FED4_ENABLE_SUBMODULE 1
#endif

// Set to 1 to skip waitUntil() poke logData (flicker A/B). 0 = normal SD logging.
// PSV2 stays on in light sleep — card keeps power (no wake remount).
#ifndef FED4_DIAG_SKIP_SD_LOG
#define FED4_DIAG_SKIP_SD_LOG 0
#endif

// Board Version: v1.7
#define FED4_BOARD_VERSION_STR "1.7.0"

// Display Colors and Constants
static const uint8_t DISPLAY_BLACK = 0;
static const uint8_t DISPLAY_WHITE = 1;
static const uint8_t DISPLAY_INVERSE = 2;
static const uint8_t DISPLAY_NORMAL = 3;

// Common display dimensions (Kyocera TN0216 physical: 320 source × 176 gate)
static const uint16_t DISPLAY_WIDTH = 320;
static const uint16_t DISPLAY_HEIGHT = 176;
// Default boot rotation until orientScreen() runs (see FED4_DisplayOrient.h)
static const uint8_t DISPLAY_ROTATION = FED4_DISPLAY_ROTATION_NATIVE;

static const uint8_t NUM_STRIP_LEDS = 8;
static const uint16_t MOTOR_STEPS = 512;
static const uint8_t MOTOR_SPEED = 24;

// TOUCH_THRESHOLD (rise fraction) is defined in FED4_TouchHelpers.h
static const char *META_FILE = "/meta.json";

static const char *PREFS_NAMESPACE = "fed4";
static const bool PREFS_RO_MODE = true;
static const bool PREFS_RW_MODE = false;

/** Why waitUntil() / startSleep() returned to the sketch. */
enum class FedWakeSource : uint8_t
{
    None = 0,
    Touch,
    Button,
    Timer,    // UI update interval deadline
    Interrupt // INT_OR GPIO (non-button)
};

/** Touch pad identity for FedEvent::pad. */
enum class FedPad : uint8_t
{
    None = 0,
    Left = 1,
    Center = 2,
    Right = 3
};

struct FedEvent
{
    FedWakeSource source = FedWakeSource::None;
    FedPad pad = FedPad::None;
    uint8_t button = 0; // 1/2/3 when Button
};

// current very public-oriented, consider pushing some to private
class FED4 : public Adafruit_GFX
{
public:
    // Constructor declaration only
    FED4();
    static const char libraryVer[];

    // Initialization
    bool begin(const char *programName = nullptr);

    // Hublink integration
    bool useHublink = false; // Default to false, can be set by user
    bool initializeHublink();
    void syncHublink();
    static void onHublinkTimestampReceived(uint32_t timestamp);

    // Motion sensor (PIR EKMB1107112) control
    bool useMotionSensor = true; // Default to true, can be set to false to disable PIR

    // Button functions
    bool initializeButtons();
    static void IRAM_ATTR onButton1WakeUp();
    static void IRAM_ATTR onButton2WakeUp();
    static void IRAM_ATTR onButton3WakeUp();
    void checkButton1();
    void checkButton2();
    void checkButton3();

    // Corefunctions
    void feed();
    void run(); // legacy: update() + sleep(sleepSeconds)
    /** Refresh clock/display/serial/hublink — call after feed() or when UI must change. */
    void update();
    /**
     * Light-sleep until touch, button, or updateIntervalSeconds.
     * MIP VCOM is kept alive by LEDC during sleep (no CPU wake chunks).
     * Calls update() before returning. Default interval 60 s.
     */
    FedEvent waitUntil(uint32_t updateIntervalSeconds = 60);

    // Game functions
    void pong();

    // Sleep configuration
    int sleepSeconds = 4;   // how many seconds to sleep between timer based wake-ups (run/sleep)
    bool sleepyLEDs = true; // Flag to control whether LEDs stay on during sleep (true = LEDs sleep with sleep, false = LEDs stay on during sleep)

    // Menu functions
    void menu();
    void menuStart();
    void menuProgram();
    void menuMouseId();
    void menuSex();
    void menuStrain();
    void menuAge();
    void menuAudio();
    void menuRTC();
    void menuEnd();

    // Sensor polling (BME/battery/lux for UI — called from update())
    void refreshSensors();
    void startupPollSensors();
    /** @deprecated Use refreshSensors(); kept as alias for older sketches. */
    void pollSensors(int minToUpdateSensors = 10);

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
    /**
     * If pendingRetrieval and well is empty, log LatePelletTaken.
     * Called from waitUntil()/feed() after PSV2 is back on. Coarse timing
     * (up to waitUntil interval); PSV2 stays off in light sleep for battery life.
     */
    bool checkLateRetrieval();
    unsigned long pelletDropTime;
    unsigned long pelletWellTime;
    bool dispenseError = false;
    void handleJams();

    // TRRS input/output connector functions
    bool initializeTRRS();
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
    void vibrateJamClear();
    void jammed();

    // Timeout functionality (defined in FED4_Timeout.cpp)
    void timeout(uint16_t min, uint16_t max);

    // Haptic motor vibration stimuli
    void hapticBuzz(uint8_t duration = 100);
    void hapticDoubleBuzz(uint8_t duration = 25);
    void hapticTripleBuzz(uint8_t duration = 5);
    void hapticRumble(uint16_t duration_ms = 300);

    // Touch sensor management (defined in FED4_Touch.cpp; free helpers in FED4_TouchHelpers.h)
    bool initializeTouch();
    void calibrateTouchSensors(bool checkStability = false);
    /** Identify active poke (rise fraction); sets flags/counts/pokeDuration/wakePad. */
    bool capturePoke();
    void resetTouchFlags();
    /** 0=none, 1=left, 2=center, 3=right — sync of FedPad after capturePoke; not an ISR latch. */
    static uint8_t wakePad;

    // Status LED and Strip control (defined in FED4_LEDs.cpp)
    // (strip - front RGB LEDs on PSV3 rail)
    bool initializeStrip();
    void setStripBrightness(uint8_t brightness);
    void colorWipe(const char *colorName, unsigned long wait);
    void colorWipe(uint32_t color, unsigned long wait);
    void stripTheaterChase(const char *colorName, unsigned long wait, unsigned int groupSize = 3, unsigned int numChases = 10);
    void stripTheaterChase(uint32_t color, unsigned long wait, unsigned int groupSize = 3, unsigned int numChases = 10);
    void stripRainbow(unsigned long wait, unsigned int numLoops);
    void randomMotion(float motionStrength, uint32_t color, unsigned long frameDelay = 75, unsigned long durationMs = 3000);
    void randomMotion(float motionStrength, const char *colorName = "blue", unsigned long frameDelay = 75, unsigned long durationMs = 3000);
    void lightsOff();
    void setStripPixel(uint8_t pixel, uint32_t color);
    void leftLight(uint32_t color);
    void leftLight(uint32_t color, uint8_t brightness);
    void centerLight(uint32_t color);
    void centerLight(uint32_t color, uint8_t brightness);
    void rightLight(uint32_t color);
    void rightLight(uint32_t color, uint8_t brightness);
    void setStripPixel(uint8_t pixel, const char *colorName);
    void leftLight(const char *colorName);
    void leftLight(const char *colorName, uint8_t brightness);
    void centerLight(const char *colorName);
    void centerLight(const char *colorName, uint8_t brightness);
    void rightLight(const char *colorName);
    void rightLight(const char *colorName, uint8_t brightness);
    // (status LED — digital red on STATUS_LED pin; LEDC VCOM owns the PWM path)
    bool initializePixel();
    void redPix(uint8_t brightness = 5);
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
    void displayAudio();
    void displayCounters();
    void displayDateTime();
    void displayEnvironmental();
    void displayBattery();
    void displaySDCardStatus();
    void displayIndicators();
    void startupAnimation();
    void displayInitStatus(const char *message);
    void displayLowBatteryWarning();
    void displayActivityMonitor();
    void displayActivityCounters();
    void displayReset();
    void displayLight(bool on);
    /** One-shot accel read; sets rotation from device X (g). Returns true if rotation changed. */
    bool orientScreen();
    /** LEDC VCOM KEEP_ALIVE (~30 Hz; Test A sleep path). */
    bool startVcomLedc();
    /** Stop LEDC VCOM and drive pin LOW (required before RST HIGH). */
    void stopVcomLedc();
    /** If LEDC was running, release pin to GPIO at last refresh() polarity. */
    void releaseVcomLedcToGpio();

    void serialStatusReport();

    // Sleep management (defined in FED4_Sleep.cpp)
    void sleep(int seconds);
    void sleep();
    void startSleep();
    void wakeUp();
    FedWakeSource lastWakeSource = FedWakeSource::None;
    unsigned long pollSensorsTimer = 0;

    // Power management (defined in FED4_Power.cpp)
    bool initializePower();
    void PSV2_ON();
    void PSV2_OFF();
    void PSV3_ON();
    void PSV3_OFF();

    // SD card functions (defined in FED4_SD.cpp)
    bool initializeSD();
    bool createMetaJson();
    bool createLogFile();
    bool logData(const String &newEvent = "");
    String getMetaValue(const char *rootKey, const char *subKey);
    bool setMetaValue(const char *rootKey, const char *subKey, const char *value);
    void setProgram(String program);
    void setMouseId(String mouseId);
    void setSex(String sex);
    void setStrain(String strain);
    void setAge(String age);
    void handleSDCardError();
    bool isSDCardAvailable() const { return sdCardAvailable; }

    // Sequence display methods
    void setSequenceDisplay(const String &sequence, int index, int level);

    // Public counters and timing
    int pelletCount;
    int centerCount;
    int leftCount;
    int rightCount;
    int blockPokeCount;
    int blockPelletCount;
    int FR;
    String currentSequence;   // For display purposes
    int currentSequenceIndex; // Current position in sequence
    int currentSequenceLevel; // Current level (FR)
    int wakeCount = 0;
    bool leftTouch;
    bool centerTouch;
    bool rightTouch;
    bool motionDetected = false;  // Track motion detection status
    int motionCount = 0;          // Aggregate motion detections between 5-minute intervals
    float motionPercentage = 0.0; // Percentage of motion detections in the last 5-minute period
    int pollCount = 0;            // Track total number of polls in each 5-minute period
    unsigned long waketime;
    bool lastMotionPositive = false; // Debounce: require two consecutive positives

    // RTC functions
    bool initializeRTC();
    void updateRTC();
    DateTime now();
    void adjustRTC(uint32_t timestamp);
    void updateTime();
    bool forceRTCUpdate = false; // Set to true to force RTC update on next initialization

#if FED4_ENABLE_SUBMODULE
    // TRRS submodule (TRIG=AUDIO_TRRS_2, DATA=AUDIO_TRRS_3 half-duplex UART)
    bool senseBegin();
    bool senseSyncTime(uint32_t timeoutMs = 2000);
    void senseTrigPulse(uint32_t durationMs = 10);
    void senseTrig(bool active); // active=true drives TRIG LOW
#endif

    // Vitals functions (defined in FED4_Vitals.cpp)
    float getBatteryVoltage();
    float getBatteryPercentage();
    float getTemperature();
    float getHumidity();
    float getPressure();
    float getGasResistance();
    bool getTempAndHumidity(float &temp, float &hum);                        // Efficient combined read
    bool getAllBME680Data(float &temp, float &hum, float &pres, float &gas); // Get all BME680 values
    float getLux();
    float getWhite();
    bool initializeLightSensor();
    bool reinitializeLightSensor();

    // variables to store temp/humidity/pressure/gas and battery info so we don't have to keep pinging the chips every time
    float temperature = -1.0;
    float humidity = -1.0;
    float pressure = -1.0;
    float gasResistance = -1.0;
    float lux = -1.0;
    float white = -1.0;
    float cellVoltage = 0.0;
    float cellPercent = 0.0;
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
    void silence();
    void unsilence();
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

    // "Super Mario"-style sound effects (tone synthesis)
    void marioCoin();
    void marioJump();
    void marioPipe();
    void marioFireball();
    void marioMushroom();

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
    float pokeDuration = 0.0;
    int motorTurns;
    int reBaselineTouches;
    char filename[32];
    bool sdCardAvailable = true; // Track if SD card operations are available
    bool audioSilenced = false;  // Track if audio has been silenced

    void clearDisplay();
    void refresh();
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    // Accelerometer functions (defined in FED4_Accel.cpp)
    bool initializeAccel();
    void setAccelRange(lis3dh_range_t range);
    void setAccelPerformanceMode(lis3dh_mode_t mode);
    void setAccelDataRate(lis3dh_dataRate_t dataRate);
    bool getAccelEvent(sensors_event_t *event);
    void readAccel(float &x, float &y, float &z);
    bool accelDataReady();

    // ToF sensor functions (defined in FED4_ToF.cpp)
    bool initializeToF();
    int prox();

    // Motion sensor functions (defined in FED4_Motion.cpp) - PIR EKMB1107112
    // PIR pin configured in begin(); no protocol init required
    bool motion();
    void updateStatusLedFromMotion(); // STATUS_LED mirrors PIR (Demo-Hardware)
    void resetMotionCounters();

    // Drop sensor functions
    bool initializeDropSensor();

    // Solenoid functions (defined in FED4_Begin.cpp)
    bool initializeSolenoids();
    void solenoid(uint8_t num, bool state);

    // ── Interrupt subsystem (defined in FED4_Interrupts.cpp) ──────────────────
    // INT_OR is the active-LOW AND of all open-drain sensor interrupts
    // and ACCEL_INT1 (push-pull, configured active-LOW).

    // Bit flags identifying each interrupt source
    enum FED4IntSource : uint8_t
    {
        INT_SRC_NONE = 0,
        INT_SRC_TOF = 1 << 0,     // VL53L1X data-ready / threshold
        INT_SRC_RTC = 1 << 1,     // DS3231 alarm 1 or alarm 2
        INT_SRC_BATTERY = 1 << 2, // MAX17048 voltage / SOC alert
        INT_SRC_ACCEL = 1 << 3,   // LIS2DH12TR inertial event on INT1
    };

    bool initializeInterrupts();               // configure INT_OR wake + accel INT1
    bool interruptPending();                   // true when INT_OR is LOW
    uint8_t scanInterrupts();                  // bitmask of all asserted sources (no clear)
    void clearInterrupts(uint8_t mask = 0xFF); // clear latches for given sources
    uint8_t scanAndClearInterrupts();          // scan + clear + verify line release
    FED4IntSource firstInterruptSource();      // highest-priority single source
    uint8_t getLastInterruptMask();            // mask captured automatically on GPIO wake
    /** Serial dump of INT_OR + button wake pins + decoded scanInterrupts() mask. */
    void printInterruptStatus(const char *tag = nullptr);

    // Opt-in per-source interrupt enable helpers
    bool enableAccelInterrupt(float threshold_g = 0.1f, uint8_t duration_count = 0);
    bool enableRTCAlarmInterrupt(uint8_t alarmNum = 1);          // arm DS3231 alarm on INT pin
    bool enableBatteryAlert(float minVoltage, float maxVoltage); // set MAX17048 VALERT window

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
    Adafruit_BME680 bme;
    Stepper stepper;
    CRGB strip_leds[NUM_STRIP_LEDS];
    Adafruit_LIS3DH accel;
    Adafruit_VEML7700 lightSensor;
    I2SClass i2s; // New I2S driver object for ESP32 core 3.x

// Hublink integration
#ifndef FED4_EXCLUDE_HUBLINK
    Hublink hublink;
#endif
    // Device state variables
    esp_adc_cal_characteristics_t *adc_cal;
    uint32_t millivolts;
    String program;
    String mouseId;
    String sex;
    String strain;
    String age;
    bool dropSensorAvailable;        // Flag to store drop sensor availability status
    uint8_t lastInterruptMask = 0;   // captured by wakeUp() on INT_OR GPIO wake
    uint8_t statusLedBrightness = 0; // Current PWM brightness for STATUS_LED
    bool pendingRetrieval = false;   // pellet still in well after awake 20 s window
    void monitorPelletInWell(uint32_t retrievalTimeoutSec);

    // RTC functions
    Preferences preferences;
    String getCompileDateTime();
    bool isNewCompilation();
    void updateCompilationID();

    uint8_t *displayBuffer = nullptr;
    bool vcom;
    bool vcomLedcActive = false;

    // I2C bus helpers (ESP32 Wire.setTimeOut — not Stream setTimeout)
    void i2cReinitBus();
    bool i2cProbe(uint8_t addr);
    void i2cRecoverBus();
    bool i2cBusHealthy();

    // Shared SPI: after SD, restore 1 MHz / LSBFIRST / CS idle for MIP
    void reclaimSpiForDisplay();
};

// Standard ASCII 5x7 font
static const unsigned char font[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, // space
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    // ... rest of font data ...
};

#endif
