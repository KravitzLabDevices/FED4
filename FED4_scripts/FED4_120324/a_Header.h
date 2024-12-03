/********************************************************
  Include libraries
********************************************************/
#include <Adafruit_MCP23X17.h>
#include "Adafruit_MAX1704X.h"
#include <Arduino.h>
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
// #include "Audio.h"
#include "WiFi.h"
#include "FS.h"
#include <SPI.h>
#include <driver/adc.h>
#include <driver/i2s.h>
#include <driver/rtc_io.h>
#include <driver/touch_pad.h>

/********************************************************
  Fuel gauge
********************************************************/
Adafruit_MAX17048 maxlipo;

/********************************************************
  RTC
********************************************************/
RTC_DS3231 rtc;

/********************************************************
  Pin definitions
********************************************************/
#define SDA_2 20
#define SCL_2 19

#define SHARP_SCK 12  //GPIO
#define SHARP_MOSI 11
#define SHARP_SS 17

#define LDO3 14  //MCP

#define haptic 8
#define PIN 35

#define PG1 12  //MCP

#define TOUCH_PIN_1 T1
#define TOUCH_PIN_5 T5
#define TOUCH_PIN_6 T6

// Define baselines and threshold for detection (lower is more sensitive, moderate sensitivity is 5000, 500 is very sensitive)
int baseline1, baseline5, baseline6;
int threshold = 300;

// ISR to handle wake-up
void IRAM_ATTR onWakeUp() {
  // Empty function to serve as interrupt handler for touchpad wake-up
}

/********************************************************
  Global variables
********************************************************/
int PG1Read;
int PG2Read;
int PG3Read;
int PG4Read;
unsigned long waketime = millis();
int FED4 = 1;
int PelletCount = 0;
int CenterCount = 0;
int LeftCount = 0;
int RightCount = 0;
int WakeCount = 0;
bool pelletReady = true;
char Event[12];
int RetrievalTime = 0;
bool FeedReady = false;

/********************************************************
  Audio stuff
********************************************************/


/********************************************************
  SD card stuff
********************************************************/
//SdFat SD;  //Make SdFat work with standard SD.h sketches
// char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

/********************************************************
  Battery voltage
********************************************************/
#define VBAT_ADC_CHANNEL ADC1_CHANNEL_6
esp_adc_cal_characteristics_t *adc_cal;
uint32_t millivolts = 0;

/********************************************************
  Front LEDs
********************************************************/


/********************************************************
  Temp/Humidity sensor
********************************************************/
TwoWire I2C_2 = TwoWire(1);
Adafruit_AHTX0 aht;

/********************************************************
  Display
********************************************************/
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168);
#define BLACK 0
#define WHITE 1

/********************************************************
  Right side pixel
********************************************************/
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

/********************************************************
  GPIO expander
********************************************************/
Adafruit_MCP23X17 mcp;

/********************************************************
  Logfile
********************************************************/
File logfile;       // Create file object
char filename[26];  // Array for file name

/********************************************************
  Stepper
********************************************************/
#define STEPS 512
Stepper stepper(STEPS, 46, 37, 21, 38);