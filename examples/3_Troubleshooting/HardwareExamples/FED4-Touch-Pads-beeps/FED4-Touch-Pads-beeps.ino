#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <driver/i2s.h>

// Define the I2S pins for audio output
#define I2S_DATA_IN_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SD_PIN 42
// Touchpad pin definitions
#define TouchPad 5
#define TouchPad2 6
#define TouchPad3 1

// NeoPixel (RGB LED) setup
#define PIN 35
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Define debounce delay
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Audio object
Audio audio;

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
      size_t bytes_written;
      i2s_write(I2S_NUM_0, sampleBuffer, sizeof(sampleBuffer), &bytes_written, portMAX_DELAY);
      samplesInBuffer = 0;
    }
  }

  // Write any remaining samples
  if (samplesInBuffer > 0) {
    size_t bytes_written;
    i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }

  // Clear the I2S buffer after generating the tone
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// I2S setup for audio
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

