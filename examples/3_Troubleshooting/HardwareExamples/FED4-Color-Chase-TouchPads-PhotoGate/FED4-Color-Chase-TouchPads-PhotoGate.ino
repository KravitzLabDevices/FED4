#include <Arduino.h>
#include <Wire.h>
#include <FastLED_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include <ESP_I2S.h>
#include <cmath>

/*
 * FED4 Color Chase + Touch Pads + Photogate test
 *
 * Updated for current hardware:
 * - Touch pads use ESP32 touch channels (NUM1/NUM2/NUM3)
 * - Photogates are direct GPIO pins (not MCP pins)
 * - Front RGB strip is on PSV3 rail (enable via MCP pin 12)
 * - Speaker amp SD is on MCP pin 4, and amp rail is PSV2 (MCP pin 13)
 * - I2S pins: BCLK=41, LRCLK=40, DIN=42, mono @ 48kHz
 */

// I2C + MCP
#define SDA_PIN 8
#define SCL_PIN 9
#define EXP_PSV2_EN 13
#define EXP_PSV3_EN 12
#define EXP_AMP_SD 4

// Front RGB strip
#define RGB_STRIP_PIN 36
#define NUM_LEDS 8
#define BRIGHTNESS 50
FastLED_NeoPixel<NUM_LEDS, RGB_STRIP_PIN, NEO_GRB> strip;

// I2S (speaker)
#define AMP_BCLK 41
#define AMP_LRCLK 40
#define AMP_DIN 42

// Sensors
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM3
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM2
#define TOUCH_PAD_LEFT TOUCH_PAD_NUM1
#define PHOTOGATE_CENTER 14

Adafruit_MCP23X17 mcp;
I2SClass i2s;

uint32_t currentColor = 0xFF0000; // Red default
uint32_t currentFrequency = 1000;
bool eventTonePlayed = false;

uint16_t baseLeft = 0;
uint16_t baseCenter = 0;
uint16_t baseRight = 0;
static constexpr float TOUCH_THRESHOLD = 0.20f; // 20% deviation from baseline

void setupI2S() {
  i2s.setPins(AMP_BCLK, AMP_LRCLK, AMP_DIN);
  if (!i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S");
  }
}

void calibrateTouch() {
  delay(50);
  baseLeft = touchRead(TOUCH_PAD_LEFT);
  baseCenter = touchRead(TOUCH_PAD_CENTER);
  baseRight = touchRead(TOUCH_PAD_RIGHT);

  Serial.printf("Touch baselines - L:%u C:%u R:%u\n", baseLeft, baseCenter, baseRight);
}

bool isTouched(uint16_t current, uint16_t baseline) {
  if (baseline == 0) return false;
  float dev = fabs((float)current / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void generateSineWave(uint32_t frequency, uint32_t duration_ms, float amplitude = 0.25f) {
  const uint32_t sampleRate = 48000;
  const uint32_t originalSamples = (sampleRate * duration_ms) / 1000;
  const uint32_t sampleCount = (originalSamples < 256) ? 256 : originalSamples;
  const float twoPiF = 2.0f * (float)M_PI * (float)frequency;

  int16_t sampleBuffer[256];
  size_t samplesInBuffer = 0;

  for (uint32_t i = 0; i < sampleCount; i++) {
    if (i < originalSamples) {
      float sample = amplitude * sinf((twoPiF * (float)i) / (float)sampleRate);
      sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767.0f);
    } else {
      sampleBuffer[samplesInBuffer++] = 0;
    }

    if (samplesInBuffer >= 256) {
      i2s.write((uint8_t *)sampleBuffer, sizeof(sampleBuffer));
      samplesInBuffer = 0;
    }
  }

  if (samplesInBuffer > 0) {
    i2s.write((uint8_t *)sampleBuffer, samplesInBuffer * sizeof(int16_t));
  }
}

void playBeep(uint32_t frequency, uint32_t duration_ms) {
  mcp.digitalWrite(EXP_AMP_SD, HIGH);
  delay(1);
  generateSineWave(frequency, duration_ms);
  delayMicroseconds(500);
  mcp.digitalWrite(EXP_AMP_SD, LOW);
}

void colorChaseWithTrail(uint32_t color) {
  static int pos = 0;

  strip.clear();
  strip.setPixelColor(pos, color);

  for (int i = 1; i <= 5; i++) {
    int trailPos = (pos - i + NUM_LEDS) % NUM_LEDS;
    uint8_t r = (uint8_t)((((color >> 16) & 0xFF) * (5 - i)) / 20);
    uint8_t g = (uint8_t)((((color >> 8) & 0xFF) * (5 - i)) / 20);
    uint8_t b = (uint8_t)(((color & 0xFF) * (5 - i)) / 20);
    strip.setPixelColor(trailPos, strip.Color(r, g, b));
  }

  strip.show();
  pos = (pos + 1) % NUM_LEDS;
  delay(170);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1) delay(10);
  }

  // Enable rails required by this test.
  mcp.pinMode(EXP_PSV2_EN, OUTPUT); // amp rail
  mcp.pinMode(EXP_PSV3_EN, OUTPUT); // front LED strip rail
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);

  // Amp SD via MCP.
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_AMP_SD, LOW);

  // Photogate input is direct GPIO on new hardware.
  pinMode(PHOTOGATE_CENTER, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  setupI2S();
  calibrateTouch();

  Serial.println("FED4 ColorChase+Touch+Photogate test ready");
}

void loop() {
  uint16_t tLeft = touchRead(TOUCH_PAD_LEFT);
  uint16_t tCenter = touchRead(TOUCH_PAD_CENTER);
  uint16_t tRight = touchRead(TOUCH_PAD_RIGHT);
  bool pgBlocked = (digitalRead(PHOTOGATE_CENTER) == LOW);

  bool touchLeft = isTouched(tLeft, baseLeft);
  bool touchCenter = isTouched(tCenter, baseCenter);
  bool touchRight = isTouched(tRight, baseRight);

  if (touchLeft) {
    currentColor = 0xFF0000;
    currentFrequency = 1000;
    if (!eventTonePlayed) {
      playBeep(currentFrequency, 200);
      eventTonePlayed = true;
    }
  } else if (touchCenter) {
    currentColor = 0x00FF00;
    currentFrequency = 1200;
    if (!eventTonePlayed) {
      playBeep(currentFrequency, 200);
      eventTonePlayed = true;
    }
  } else if (touchRight) {
    currentColor = 0x0000FF;
    currentFrequency = 1500;
    if (!eventTonePlayed) {
      playBeep(currentFrequency, 200);
      eventTonePlayed = true;
    }
  } else if (pgBlocked) {
    currentColor = 0xBB0BD3;
    currentFrequency = 800;
    if (!eventTonePlayed) {
      playBeep(currentFrequency, 200);
      eventTonePlayed = true;
    }
  } else {
    eventTonePlayed = false;
  }

  colorChaseWithTrail(currentColor);
}
