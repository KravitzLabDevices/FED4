/*
 * FED4 Speaker with SD Test
 *
 * Button 1: play /Audio/Beep.mp3 from SD card
 * Button 2: stop MP3 and play two generated tones
 */

#include <Arduino.h>
#include <Wire.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ESP_I2S.h>
#include <Adafruit_MCP23X17.h>
#include "Audio.h"
#include <cmath>

#define SDA_PIN 8
#define SCL_PIN 9

#define AMP_BCLK 41
#define AMP_LRCLK 40
#define AMP_DIN 42

#define BUTTON_1_PIN 15
#define BUTTON_2_PIN 16

#define SD_CS 48
#define SPI_SCK 12
#define SPI_MISO 11
#define SPI_MOSI 13

#define EXP_AMP_SD 4
#define EXP_PSV2_EN 13

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

Adafruit_MCP23X17 mcp;
Audio audio;
I2SClass i2s;

void setupI2S() {
  i2s.setPins(AMP_BCLK, AMP_LRCLK, AMP_DIN);
  if (!i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S");
  }
}

void resetI2S() {
  i2s.end();
  setupI2S();
}

void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
  const uint32_t sampleRate = 48000;
  const uint32_t sampleCount = max((sampleRate * duration_ms) / 1000, (uint32_t)256);
  const float amplitude = 0.25f;
  const float twoPiF = 2.0f * (float)M_PI * frequency;
  int16_t sampleBuffer[256];
  size_t n = 0;

  for (uint32_t i = 0; i < sampleCount; i++) {
    float s = (i < (sampleRate * duration_ms) / 1000)
              ? amplitude * sinf((twoPiF * i) / sampleRate)
              : 0.0f;
    sampleBuffer[n++] = (int16_t)(s * 32767.0f);
    if (n >= 256) {
      i2s.write((uint8_t *)sampleBuffer, sizeof(sampleBuffer));
      n = 0;
    }
  }
  if (n > 0) i2s.write((uint8_t *)sampleBuffer, n * sizeof(int16_t));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP init failed");
    while (1) delay(10);
  }

  // Enable PSV2 rail and amp control line.
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_AMP_SD, HIGH);

  pinMode(BUTTON_1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_2_PIN, INPUT_PULLDOWN);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  if (!SD.begin(SD_CS, SPI, 4000000)) {
    Serial.println("Card Mount Failed");
  }

  audio.setPinout(AMP_BCLK, AMP_LRCLK, AMP_DIN, -1);
  audio.setVolume(20);
  setupI2S();

  Serial.println("Ready: BTN1=play MP3, BTN2=stop+tones");
}

void loop() {
  bool b1 = digitalRead(BUTTON_1_PIN);
  bool b2 = digitalRead(BUTTON_2_PIN);

  if (b1 && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 1: play /Audio/Beep.mp3");
    audio.connecttoFS(SD, "/Audio/Beep.mp3");
    lastDebounceTime = millis();
  }

  if (b2 && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 2: stop song + tones");
    audio.stopSong();
    resetI2S();
    generateSineWave(1000, 250);
    delay(100);
    generateSineWave(1500, 250);
    lastDebounceTime = millis();
  }

  audio.loop();
}
