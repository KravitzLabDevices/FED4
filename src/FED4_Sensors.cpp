#include "FED4.h"
#include "driver/touch_pad.h"

// Initialize the static member
uint8_t FED4::wakePad = 0;  // 0=none, 1=left, 2=center, 3=right

void IRAM_ATTR FED4::onTouchWakeUp()
{
    // Get the touch pad that triggered the wake-up
    uint32_t touch_status = touch_pad_get_status();
    
    // Store which pad triggered the wake-up
    if (touch_status & (1 << TOUCH_PAD_LEFT)) {
        wakePad = 1;
    } else if (touch_status & (1 << TOUCH_PAD_CENTER)) {
        wakePad = 2;
    } else if (touch_status & (1 << TOUCH_PAD_RIGHT)) {
        wakePad = 3;
    }
    
    // Clear the touch pad status
    touch_pad_clear_status();
}

bool FED4::initializeTouch()
{
    esp_err_t err = touch_pad_init();
    if (err != ESP_OK)
    {
        return false;
    }

    // Configure touch pads
    err = touch_pad_config(TOUCH_PAD_LEFT);
    if (err != ESP_OK)
        return false;

    err = touch_pad_config(TOUCH_PAD_CENTER);
    if (err != ESP_OK)
        return false;

    err = touch_pad_config(TOUCH_PAD_RIGHT);
    if (err != ESP_OK)
        return false;

    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    delay(50);  // Reduced from 200ms to 50ms - minimum time needed for voltage stabilization
    return true;
}

void FED4::calibrateTouchSensors()
{
    // Detach existing interrupts to prevent memory leaks
    touchDetachInterrupt(TOUCH_PAD_LEFT);
    touchDetachInterrupt(TOUCH_PAD_CENTER);
    touchDetachInterrupt(TOUCH_PAD_RIGHT);

    touchPadLeftBaseline = touchRead(TOUCH_PAD_LEFT);
    touchPadCenterBaseline = touchRead(TOUCH_PAD_CENTER);
    touchPadRightBaseline = touchRead(TOUCH_PAD_RIGHT);

    uint16_t left_threshold = touchPadLeftBaseline * TOUCH_THRESHOLD;
    uint16_t center_threshold = touchPadCenterBaseline * TOUCH_THRESHOLD;
    uint16_t right_threshold = touchPadRightBaseline * TOUCH_THRESHOLD;

    // Serial.printf("Touch sensor thresholds - Left: %d, Center: %d, Right: %d\n",
    //               left_threshold, center_threshold, right_threshold);

    // Enable wake-up on touch pads
    esp_sleep_enable_touchpad_wakeup();

    // Set individual thresholds for each pad
    touchAttachInterrupt(TOUCH_PAD_LEFT, onTouchWakeUp, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onTouchWakeUp, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onTouchWakeUp, right_threshold);
}

/**
 * Interprets which touch sensor was activated after waking from sleep
 * Compares readings from left, center, and right touch sensors to their baselines
 * Sets the appropriate touch flag (leftTouch, centerTouch, rightTouch) 
 * Increments the corresponding counter, logs the event, updates display and LED
 */
void FED4::interpretTouch()
{
    // Print which pad triggered the wake-up
      if (wakePad == 1) {
          Serial.print("LEFT touch   ");
          leftCount++;
          leftTouch = true;  // Set flag first for fastest response
      } else if (wakePad == 2) {
          Serial.print("CENTER touch ");
          centerCount++;
          centerTouch = true;  // Set flag first for fastest response
      } else if (wakePad == 3) {
          Serial.print("RIGHT touch  ");
          rightCount++;
          rightTouch = true;  // Set flag first for fastest response
      } 

    wakePad = 0;  // Reset the wake pad flag
    // Clear any pending touch pad interrupts
    displayIndicators();
    touch_pad_clear_status();
}

void FED4::resetTouchFlags()
{
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
}