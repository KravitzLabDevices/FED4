#include <FastLED_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <ESP_I2S.h>  // New I2S API for ESP32 core 3.x
Adafruit_MCP23X17 mcp;
#define DATA_PIN 36
#define NUM_LEDS 8
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];
FastLED_NeoPixel<NUM_LEDS, DATA_PIN, NEO_GRB> strip;

#define I2S_DATA_IN_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SD_PIN 42

#define TouchPad1 5
#define TouchPad2 6
#define TouchPad3 1
#define PG1 12
Audio audio;
I2SClass i2s;  // New I2S object for tone generation
uint32_t currentColor = 0xFF0000; // Default color (red)
uint32_t currentFrequency = 1000; // Default frequency for the beep
bool beepPlayed = false; // Flag to track if beep has been played for the touch

void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
    const uint32_t sampleRate = 44100;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
    const float amplitude = 0.5;
    const float twoPiF = 2.0 * M_PI * frequency;

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
    if (samplesInBuffer > 0) {
        i2s.write((uint8_t*)sampleBuffer, samplesInBuffer * sizeof(int16_t));
    }
}

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

	mcp.pinMode(PG1, INPUT_PULLUP);
    strip.begin();
    strip.setBrightness(BRIGHTNESS);

    pinMode(I2S_SD_PIN, OUTPUT);
    digitalWrite(I2S_SD_PIN, HIGH);
    audio.setPinout(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_IN_PIN, -1);
    audio.setVolume(30);
    setupI2S();
}

// Function to chase a color with trailing effect
void colorChaseWithTrail(uint32_t color) {
    static int pos = 0;

    // Clear all LEDs first to turn off any LEDs not in the trail
    strip.clear();

    // Set the current LED to the active color at full brightness
    strip.setPixelColor(pos, color);

    // Create trailing LEDs with different shades
    for (int i = 1; i <= 5; i++) {
        int trailPos = (pos - i + NUM_LEDS) % NUM_LEDS;
        // Subvariant shade of the main color for trailing effect
        uint32_t shadeColor = strip.Color(
            ((color >> 16) * (5 - i)) / 20, // Red channel (darker shade)
            (((color >> 8) & 0xFF) * (5 - i)) / 20, // Green channel (darker shade)
            ((color & 0xFF) * (5 - i)) / 20 // Blue channel (darker shade)
        );
        strip.setPixelColor(trailPos, shadeColor);
    }

    // Display the LEDs
    strip.show();

    // Move to the next LED position
    pos = (pos + 1) % NUM_LEDS;
    delay(170); // Adjust the speed of the chase
}

void loop() {
    int t1 = touchRead(TouchPad1);
    int t2 = touchRead(TouchPad2);
    int t3 = touchRead(TouchPad3);
    int PG1Read = mcp.digitalRead(PG1);

    bool touch1 = t1 >= 50000;
    bool touch2 = t2 >= 50000;
    bool touch3 = t3 >= 50000;

    // Update color and frequency based on touchpad input and trigger beep only once
    if (touch1) {
        currentColor = 0xFF0000; // Red for TouchPad1
        currentFrequency = 1000; // 1000 Hz beep for TouchPad1
        if (!beepPlayed) {
            generateSineWave(currentFrequency, 200);
            beepPlayed = true;
        }
    }
    else if (touch2) {
        currentColor = 0x00FF00; // Green for TouchPad2
        currentFrequency = 1200; // 1200 Hz beep for TouchPad2
        if (!beepPlayed) {
            generateSineWave(currentFrequency, 200);
            beepPlayed = true;
        }
    }
    else if (touch3) {
        currentColor = 0x0000FF; // Blue for TouchPad3
        currentFrequency = 1500; // 1500 Hz beep for TouchPad3
        if (!beepPlayed) {
            generateSineWave(currentFrequency, 200);
            beepPlayed = true;
        }
    }
    else if (PG1Read == LOW) {
        currentColor = 0xbb0bd3; // Blue for TouchPad3
        currentFrequency = 800; // 1500 Hz beep for TouchPad3
        if (!beepPlayed) {
            generateSineWave(currentFrequency, 200);
            beepPlayed = true;
        }
    }
    else {
        beepPlayed = false; // Reset beep flag when no touchpad is pressed
    }

    // Continuous color chase with trailing effect
    colorChaseWithTrail(currentColor);

    audio.loop();
}
