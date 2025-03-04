#include <Arduino.h>

#define TOUCH_PAD1_PIN 1     // GPIO for TouchPad1
#define TOUCH_PAD2_PIN 5     // GPIO for TouchPad2
#define TOUCH_PAD3_PIN 6     // GPIO for TouchPad3

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PAD1_PIN, INPUT);
  pinMode(TOUCH_PAD2_PIN, INPUT);
  pinMode(TOUCH_PAD3_PIN, INPUT);
  esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_PAD1_PIN) | (1ULL << TOUCH_PAD2_PIN) | (1ULL << TOUCH_PAD3_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

  Serial.println("Going to light sleep now...");
  delay(100);
}

void loop() { 
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t wakeup_pin = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pin & (1ULL << TOUCH_PAD1_PIN)) {
      Serial.println("Woke up by TOUCH_PAD1_PIN");
    } else if (wakeup_pin & (1ULL << TOUCH_PAD2_PIN)) {
      Serial.println("Woke up by TOUCH_PAD2_PIN");
    } else if (wakeup_pin & (1ULL << TOUCH_PAD3_PIN)) {
      Serial.println("Woke up by TOUCH_PAD3_PIN");
    }
      } else {
        Serial.println("Woke up by other cause");
      }
    delay(2000);
    Serial.println("Going back to sleep...");
     delay(100);
    esp_light_sleep_start(); 
}
