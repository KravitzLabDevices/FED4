#include "FED4.h"

// Initialize the static member
uint8_t FED4::wakePad = 0;  // 0=none, 1=left, 2=center, 3=right

// Separate ISR callbacks for each touch pad (avoids legacy driver API calls)
void IRAM_ATTR FED4::onTouchWakeUp()
{
    // Generic wake-up handler - wakePad is set by specific handlers below
}

void IRAM_ATTR onLeftTouch() {
    FED4::wakePad = 1;
}

void IRAM_ATTR onCenterTouch() {
    FED4::wakePad = 2;
}

void IRAM_ATTR onRightTouch() {
    FED4::wakePad = 3;
}

bool FED4::initializeTouch()
{
    // Use Arduino's touch API which handles all low-level initialization
    // The first touchRead() call will initialize the touch subsystem automatically
    // Just verify that the touch pads are responsive by doing a test read
    uint16_t testLeft = touchRead(TOUCH_PAD_LEFT);
    uint16_t testCenter = touchRead(TOUCH_PAD_CENTER);
    uint16_t testRight = touchRead(TOUCH_PAD_RIGHT);
    
    // Verify we got valid readings (non-zero values)
    if (testLeft == 0 || testCenter == 0 || testRight == 0) {
        return false;
    }
    
    delay(25);  // Allow touch subsystem to stabilize
    return true;
}

void FED4::calibrateTouchSensors()
{
    // Detach existing interrupts to prevent memory leaks
    touchDetachInterrupt(TOUCH_PAD_LEFT);
    touchDetachInterrupt(TOUCH_PAD_CENTER);
    touchDetachInterrupt(TOUCH_PAD_RIGHT);
    
    // Small delay to ensure interrupts are fully detached before reattaching
    delay(10);

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

    // Set individual thresholds for each pad with separate callbacks
    touchAttachInterrupt(TOUCH_PAD_LEFT, onCenterTouch, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onRightTouch, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onLeftTouch, right_threshold);
}

/**
 * Interprets which touch sensor was activated after waking from sleep
 * Compares readings from left, center, and right touch sensors to their baselines
 * Sets the appropriate touch flag (leftTouch, centerTouch, rightTouch) 
 * Increments the corresponding counter
 */
void FED4::interpretTouch()
{
    if (wakePad == 1) {
        leftCount++;
        leftTouch = true; 
    } else if (wakePad == 2) {
        centerCount++;
        centerTouch = true; 
    } else if (wakePad == 3) {
        rightCount++;
        rightTouch = true; 
    }
    wakePad = 0;  // Reset the wake pad flag
}

void FED4::resetTouchFlags()
{
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
}

// Optional: Add this function to log touch events separately from the critical path
void FED4::logTouchEvent()
{
    if (leftTouch) {
        Serial.print("LEFT touch   ");
    } else if (centerTouch) {
        Serial.print("CENTER touch ");
    } else if (rightTouch) {
        Serial.print("RIGHT touch  ");
    }
}