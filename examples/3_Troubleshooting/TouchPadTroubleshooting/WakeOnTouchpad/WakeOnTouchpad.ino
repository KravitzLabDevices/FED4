/*
Example: Using Touch Interrupts with Debouncing on ESP32 to wake on each touchpad
*/

#include "esp_sleep.h"
#include "driver/touch_pad.h"

int threshold = 300;                      // Adjust touch sensitivity
const unsigned long debounceDelay = 100;  // Debounce time in milliseconds

// Start global touch flags as false
volatile bool touch1detected = false;
volatile bool touch5detected = false;
volatile bool touch6detected = false;

int touchCount = 0;
unsigned long lastTouchTime = 0;  // Track the last touch time for debouncing

// Interrupt service routines for touch
void IRAM_ATTR gotTouch1() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    touch1detected = true;
    lastTouchTime = now;  // Update last touch time
  }
}

void IRAM_ATTR gotTouch5() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    touch5detected = true;
    lastTouchTime = now;  // Update last touch time
  }
}

void IRAM_ATTR gotTouch6() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    touch6detected = true;
    lastTouchTime = now;  // Update last touch time
  }
}

void setup() {
  Serial.begin(115200);
  initializeTouchpads();
}

void loop() {
  goToSleep();
  checkInput();
}

void initializeTouchpads() {
  Serial.println("Enabling touchpad wakeup...");

  // Initialize touch pad peripheral
  touch_pad_init();

  // set voltages (optional)
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

  //enable wakeup on touchpad
  esp_sleep_enable_touchpad_wakeup();

  // Configure touchpad sensitivity and attach independent interrupts to each touchpad
  touchAttachInterrupt(T1, gotTouch1, threshold);
  touchAttachInterrupt(T5, gotTouch5, threshold);
  touchAttachInterrupt(T6, gotTouch6, threshold);
}

void checkInput() {
  // Check which touchpad caused the wake-up
  if (touch1detected) {
    Serial.println("Touch 1 detected");
  }
  if (touch5detected) {
    Serial.println("Touch 5 detected");
  }
  if (touch6detected) {
    Serial.println("Touch 6 detected");
  }
  //clear touch flags
  touch1detected = false;
  touch5detected = false;
  touch6detected = false;
}

void goToSleep() {
  // Go to sleep
  Serial.println("Going to sleep...");
  Serial.println();
  delay(50);  // Let Serial print complete before sleeping
  esp_light_sleep_start();

  // Wake up here
  touchCount++;
  Serial.print("Woke up! Touches = ");
  Serial.println(touchCount);
}