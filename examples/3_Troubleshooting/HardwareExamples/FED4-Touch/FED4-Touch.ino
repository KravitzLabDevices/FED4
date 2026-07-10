/*
 * FED4 Touch Test
 *
 * Touch pads:
 *   LEFT   = TOUCH_PAD_NUM1
 *   CENTER = TOUCH_PAD_NUM3
 *   RIGHT  = TOUCH_PAD_NUM2
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <FED4_Pins.h>
#include <cmath>

#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, STATUS_LED, NEO_GRB + NEO_KHZ800);

uint16_t baseLeft = 0, baseCenter = 0, baseRight = 0;
static constexpr float TOUCH_THRESHOLD = 0.20f;

bool touched(uint16_t value, uint16_t baseline) {
  if (!baseline) return false;
  float dev = fabs((float)value / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pixels.begin();
  pixels.clear();
  pixels.show();

  delay(50);
  baseLeft = touchRead(TOUCH_PAD_LEFT);
  baseCenter = touchRead(TOUCH_PAD_CENTER);
  baseRight = touchRead(TOUCH_PAD_RIGHT);
  Serial.printf("Baselines L:%u C:%u R:%u\n", baseLeft, baseCenter, baseRight);
}

void loop() {
  uint16_t l = touchRead(TOUCH_PAD_LEFT);
  uint16_t c = touchRead(TOUCH_PAD_CENTER);
  uint16_t r = touchRead(TOUCH_PAD_RIGHT);

  bool left = touched(l, baseLeft);
  bool center = touched(c, baseCenter);
  bool right = touched(r, baseRight);

  if (left) {
    Serial.printf("LEFT touch (%u)\n", l);
    pixels.setPixelColor(0, pixels.Color(150, 0, 0));
  } else if (center) {
    Serial.printf("CENTER touch (%u)\n", c);
    pixels.setPixelColor(0, pixels.Color(0, 150, 0));
  } else if (right) {
    Serial.printf("RIGHT touch (%u)\n", r);
    pixels.setPixelColor(0, pixels.Color(0, 0, 150));
  } else {
    pixels.setPixelColor(0, 0);
  }
  pixels.show();

  delay(40);
}

