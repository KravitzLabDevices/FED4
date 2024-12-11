#include <Arduino.h>

#define TOUCH_PAD1_PIN 1     // GPIO for TouchPad1
#define TOUCH_PAD2_PIN 5     // GPIO for TouchPad2
#define TOUCH_PAD3_PIN 6     // GPIO for TouchPad3

void setup() {
  Serial.begin(115200);
  // Configure touchpad GPIO pins as inputs
  pinMode(TOUCH_PAD1_PIN, INPUT);
  pinMode(TOUCH_PAD2_PIN, INPUT);
  pinMode(TOUCH_PAD3_PIN, INPUT);

  esp_sleep_enable_gpio_wakeup();
  gpio_wakeup_enable((gpio_num_t)TOUCH_PAD1_PIN, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)TOUCH_PAD2_PIN, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)TOUCH_PAD3_PIN, GPIO_INTR_HIGH_LEVEL);

  Serial.println("Going to light sleep now...");
}
void loop() {
  delay(2000);  
  esp_light_sleep_start();
  Serial.println("Going back to sleep...");
}
