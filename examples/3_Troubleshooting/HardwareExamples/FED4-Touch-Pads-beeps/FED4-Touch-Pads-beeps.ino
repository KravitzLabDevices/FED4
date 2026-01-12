#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <ESP_I2S.h>  // New I2S API for ESP32 core 3.x
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

// Define the I2S pins for audio output
#define I2S_DATA_IN_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SD_PIN 42
// Touchpad pin definitions
#define TouchPad 5
#define TouchPad2 6
#define TouchPad3 1
#define LDO3 14
// NeoPixel (RGB LED) setup
#define PIN 35
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Define debounce delay
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Audio object
Audio audio;
I2SClass i2s;  // New I2S object for tone generation

// Function to generate beep sound
void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
  const uint32_t sampleRate = 44100;
  const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
  const float amplitude = 0.5;
  const float twoPiF = 2.0 * M_PI * frequency;

  // Buffer to store multiple samples before writing
  int16_t sampleBuffer[256];
  size_t samplesInBuffer = 0;

  for (uint32_t i = 0; i < sampleCount; i++) {
    float sample = amplitude * sin((twoPiF * i) / sampleRate);
    sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767);

    if (samplesInBuffer >= 256) {
      i2s.write((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
      samplesInBuffer = 0;
    }
  }

  // Write any remaining samples
  if (samplesInBuffer > 0) {
    i2s.write((uint8_t*)sampleBuffer, samplesInBuffer * sizeof(int16_t));
  }
}

// I2S setup for audio
void setupI2S() {
  // Configure I2S pins
  i2s.setPins(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_IN_PIN);
  
  // Initialize I2S with new API
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("Failed to initialize I2S");
  }
}

void setup() {
  Serial.begin(115200);
  	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
    if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1)
			;
	}

	mcp.pinMode(LDO3, OUTPUT);
	mcp.digitalWrite(LDO3, HIGH);
  // Setup NeoPixel
  pixels.begin();

   pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
    audio.setPinout(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_IN_PIN, -1);
  audio.setVolume(100); 
  setupI2S();
}

void loop() {
  // Read touchpad inputs
  int t = touchRead(TouchPad);
  int t2 = touchRead(TouchPad2);
  int t3 = touchRead(TouchPad3);

  // TouchPad1 (Red)
  if (t >= 50000 ) {
    pixels.setPixelColor(0, pixels.Color(150, 0, 0));  // Set RGB to red
    pixels.show();
    generateSineWave(1000, 500);  // Generate 1000 Hz beep for 100 ms
    lastDebounceTime = millis();
  }

  // TouchPad2 (Green)
  else if (t2 >= 50000 ) {
    pixels.setPixelColor(0, pixels.Color(0, 150, 0));  // Set RGB to green
    pixels.show();
    generateSineWave(1200, 500);  // Generate 1200 Hz beep for 100 ms
    lastDebounceTime = millis();
  }

  // TouchPad3 (Blue)
  else if (t3 >= 50000 ) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 150));  // Set RGB to blue
    pixels.show();
    generateSineWave(1500, 500);  // Generate 1500 Hz beep for 100 ms
    lastDebounceTime = millis();
  }
  else{
    pixels.clear();
    pixels.show();
  }
  audio.loop();
}

