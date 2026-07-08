/*
 * FED4 Touch Pads + Beeps
 */

#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>
#include <cmath>

#define SDA_PIN 8
#define SCL_PIN 9

#define AMP_BCLK 41
#define AMP_LRCLK 40
#define AMP_DIN 42

#define EXP_AMP_SD 4
#define EXP_PSV2_EN 13
#define EXP_PSV3_EN 12

#define TOUCH_PAD_LEFT TOUCH_PAD_NUM1
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM3
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM2

#define STATUS_LED_PIN 35

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel pixels(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
I2SClass i2s;

uint16_t baseL = 0, baseC = 0, baseR = 0;
bool tonePlayed = false;
static constexpr float TOUCH_THRESHOLD = 0.20f;

bool touched(uint16_t value, uint16_t baseline) {
  if (!baseline) return false;
  float dev = fabs((float)value / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
  const uint32_t sampleRate = 48000;
  const uint32_t original = (sampleRate * duration_ms) / 1000;
  const uint32_t sampleCount = max(original, (uint32_t)256);
  const float amp = 0.25f;
  const float twoPiF = 2.0f * (float)M_PI * frequency;
  int16_t buffer[256];
  size_t n = 0;

  mcp.digitalWrite(EXP_AMP_SD, HIGH);
  delay(1);

  for (uint32_t i = 0; i < sampleCount; i++) {
    float s = (i < original) ? amp * sinf((twoPiF * i) / sampleRate) : 0.0f;
    buffer[n++] = (int16_t)(s * 32767.0f);
    if (n >= 256) {
      i2s.write((uint8_t *)buffer, sizeof(buffer));
      n = 0;
    }
  }
  if (n > 0) i2s.write((uint8_t *)buffer, n * sizeof(int16_t));

  delayMicroseconds(500);
  mcp.digitalWrite(EXP_AMP_SD, LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP init failed");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_AMP_SD, LOW);

  i2s.setPins(AMP_BCLK, AMP_LRCLK, AMP_DIN);
  i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

  pixels.begin();
  pixels.clear();
  pixels.show();

  delay(50);
  baseL = touchRead(TOUCH_PAD_LEFT);
  baseC = touchRead(TOUCH_PAD_CENTER);
  baseR = touchRead(TOUCH_PAD_RIGHT);
}

void loop() {
  bool left = touched(touchRead(TOUCH_PAD_LEFT), baseL);
  bool center = touched(touchRead(TOUCH_PAD_CENTER), baseC);
  bool right = touched(touchRead(TOUCH_PAD_RIGHT), baseR);

  if (left) {
    pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    if (!tonePlayed) generateSineWave(1000, 180);
    tonePlayed = true;
  } else if (center) {
    pixels.setPixelColor(0, pixels.Color(0, 150, 0));
    if (!tonePlayed) generateSineWave(1200, 180);
    tonePlayed = true;
  } else if (right) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 150));
    if (!tonePlayed) generateSineWave(1500, 180);
    tonePlayed = true;
  } else {
    pixels.setPixelColor(0, 0);
    tonePlayed = false;
  }
  pixels.show();
  delay(30);
}

