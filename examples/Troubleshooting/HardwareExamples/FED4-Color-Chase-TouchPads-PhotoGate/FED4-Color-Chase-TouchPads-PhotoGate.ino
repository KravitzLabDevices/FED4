#include <FastLED_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <driver/i2s.h>
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
            size_t bytes_written;
            i2s_write(I2S_NUM_0, sampleBuffer, sizeof(sampleBuffer), &bytes_written, portMAX_DELAY);
            samplesInBuffer = 0;
        }
    }
    if (samplesInBuffer > 0) {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupI2S() {
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_BIT_CLOCK_PIN,
        .ws_io_num = I2S_LEFT_RIGHT_CLOCK_PIN,
        .data_out_num = I2S_DATA_IN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pinConfig);
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
