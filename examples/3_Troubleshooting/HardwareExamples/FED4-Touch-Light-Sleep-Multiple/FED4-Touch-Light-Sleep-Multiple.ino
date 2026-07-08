/*
 * FED4 Touch Light Sleep
 *
 * Uses native touch wake (not EXT1 GPIO wake).
 */

#include <Arduino.h>
#include <cmath>

#define TOUCH_PAD_LEFT TOUCH_PAD_NUM1
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM3
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM2

uint16_t baseLeft = 0, baseCenter = 0, baseRight = 0;
static constexpr float TOUCH_THRESHOLD = 0.20f;

bool touched(uint16_t value, uint16_t baseline) {
  if (!baseline) return false;
  float dev = fabs((float)value / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void printLikelyTouch() {
  uint16_t l = touchRead(TOUCH_PAD_LEFT);
  uint16_t c = touchRead(TOUCH_PAD_CENTER);
  uint16_t r = touchRead(TOUCH_PAD_RIGHT);

  bool left = touched(l, baseLeft);
  bool center = touched(c, baseCenter);
  bool right = touched(r, baseRight);

  if (left) Serial.println("Likely wake source: LEFT");
  if (center) Serial.println("Likely wake source: CENTER");
  if (right) Serial.println("Likely wake source: RIGHT");
  if (!left && !center && !right) Serial.println("No pad currently above threshold");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  delay(30);
  baseLeft = touchRead(TOUCH_PAD_LEFT);
  baseCenter = touchRead(TOUCH_PAD_CENTER);
  baseRight = touchRead(TOUCH_PAD_RIGHT);
  Serial.printf("Baselines L:%u C:%u R:%u\n", baseLeft, baseCenter, baseRight);

  esp_sleep_enable_touchpad_wakeup();
  Serial.println("Touch wake enabled. Entering light sleep...");
  delay(100);
}

void loop() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_TOUCHPAD) {
    Serial.println("Woke up by touchpad");
    printLikelyTouch();
  } else {
    Serial.printf("Wake cause: %d\n", reason);
  }

  delay(500);
  Serial.println("Going back to light sleep...");
  delay(50);
  esp_light_sleep_start();
}
