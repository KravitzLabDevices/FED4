/*
Example: Using Touch Interrupts with Debouncing on ESP32 to wake on each touchpad
*/

#include "esp_sleep.h"
#include "driver/touch_pad.h"

int threshold = 300;                      // Adjust touch sensitivity
const unsigned long debounceDelay = 100;  // Debounce time in milliseconds

// Start global touch flags as false
volatile bool leftTouchDetected = false;
volatile bool rightTouchDetected = false;
volatile bool centerTouchDetected = false;

int touchCount = 0;
unsigned long lastTouchTime = 0;  // Track the last touch time for debouncing

// Interrupt service routines for touch
void IRAM_ATTR leftTouch() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    leftTouchDetected = true;
    lastTouchTime = now;  // Update last touch time
  }
}

void IRAM_ATTR rightTouch() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    rightTouchDetected = true;
    lastTouchTime = now;  // Update last touch time
  }
}

void IRAM_ATTR centerTouch() {
  unsigned long now = millis();
  if (now - lastTouchTime > debounceDelay) {  // Debounce check
    centerTouchDetected = true;
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
  touchAttachInterrupt(T1, leftTouch, threshold);
  touchAttachInterrupt(T5, rightTouch, threshold);
  touchAttachInterrupt(T6, centerTouch, threshold);
}

void checkInput() {
  // Check which touchpad caused the wake-up
  if (leftTouchDetected) {
    Serial.println("Left Touch detected");
  }
  if (rightTouchDetected) {
    Serial.println("Right Touch detected");
  }
  if (centerTouchDetected) {
    Serial.println("Center Touch detected");
  }
  //clear touch flags
  leftTouchDetected = false;
  rightTouchDetected = false;
  centerTouchDetected = false;
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