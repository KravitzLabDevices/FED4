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
#include <SdFat.h>
#include <Adafruit_AHTX0.h>
#include "WiFi.h"
#include "FS.h"
#include <SPI.h>
#include <driver/adc.h>
#include <driver/i2s.h>
#include <driver/rtc_io.h>
#include <driver/touch_pad.h>

// Pin Definitions
#define SDA_2 20
#define SCL_2 19
#define SHARP_SCK 12
#define SHARP_MOSI 11
#define SHARP_SS 17
#define LDO3 14
#define haptic 8
#define PIN 35
#define PG1 12
#define TOUCH_PIN_1 T1
#define TOUCH_PIN_5 T5
#define TOUCH_PIN_6 T6
#define NUMPIXELS 1
#define STEPS 512
#define VBAT_ADC_CHANNEL ADC1_CHANNEL_6
#define BLACK 0
#define WHITE 1

// TODO: These pins need proper naming and verification
#define MOTOR_PIN_1 46 // Used in ReleaseMotor() and Feed()
#define MOTOR_PIN_2 37 // Used in ReleaseMotor() and Feed()
#define MOTOR_PIN_3 21 // Used in ReleaseMotor() and Feed()
#define MOTOR_PIN_4 38 // Used in ReleaseMotor() and Feed()
#define POWER_PIN_1 47 // Used in begin() and enterLightSleep()
#define POWER_PIN_2 36 // Used in enterLightSleep()
#define GPIO_PIN_1 2   // Used in begin()
#define GPIO_PIN_2 3   // Used in begin()
#define GPIO_PIN_3 4   // Used in begin()

class FED4
{
public:
    // Constructor
    FED4();

    // Initialization
    void begin();

    // Core functionality
    void Feed();
    void CheckForPellet();
    void UpdateDisplay();
    void SerialOutput();
    void enterLightSleep();

    // Touch sensor management
    void CalibrateTouchSensors();
    void BaselineTouchSensors();
    void interpretTouch();

    // LED control and stimuli
    void BluePix();
    void DimBluePix();
    void GreenPix();
    void RedPix();
    void PurplePix();
    void NoPix();
    void Vibrate(unsigned long wait);

    // Public counters and timing
    int PelletCount;
    int CenterCount;
    int LeftCount;
    int RightCount;
    int WakeCount;
    unsigned long waketime;

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

    // Private methods
    void ReleaseMotor();
    void touch_pad_init();
    static void IRAM_ATTR onWakeUp();
};

#endif