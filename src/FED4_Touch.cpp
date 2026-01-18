#include "FED4.h"
#include "esp_timer.h"

// Initialize the static members
uint8_t FED4::wakePad = 0;  // 0=none, 1=left, 2=center, 3=right
static volatile int64_t lastCalibrationTime_us = 0;  // Timestamp of last calibration in microseconds

// Separate ISR callbacks for each touch pad (avoids legacy driver API calls)
void IRAM_ATTR FED4::onTouchWakeUp()
{
    // Generic wake-up handler - wakePad is set by specific handlers below
}

// ISR callbacks for touch pad interrupts - names match the physical pad they're attached to
// Inhibit triggers within 10ms of calibration to prevent false triggers
void IRAM_ATTR onLeftPadTouch() {
    // Ignore triggers within 10ms (10000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 10000) {
        return;  // Too soon after calibration, ignore this trigger
    }
    FED4::wakePad = 1;  // LEFT pad
}

void IRAM_ATTR onCenterPadTouch() {
    // Ignore triggers within 10ms (10000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 10000) {
        return;  // Too soon after calibration, ignore this trigger
    }
    FED4::wakePad = 2;  // CENTER pad
}

void IRAM_ATTR onRightPadTouch() {
    // Ignore triggers within 10ms (10000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 10000) {
        return;  // Too soon after calibration, ignore this trigger
    }
    FED4::wakePad = 3;  // RIGHT pad
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

void FED4::calibrateTouchSensors(bool checkStability)
{
    // Detach existing interrupts to prevent memory leaks
    touchDetachInterrupt(TOUCH_PAD_LEFT);
    touchDetachInterrupt(TOUCH_PAD_CENTER);
    touchDetachInterrupt(TOUCH_PAD_RIGHT);
    
    // Small delay to ensure interrupts are fully detached before reattaching
    delay(10);

    // Clear any pending wakePad flags to prevent stale interrupt triggers
    wakePad = 0;

    // Disable interrupts during critical calibration section to prevent false triggers
    noInterrupts();

    touchPadLeftBaseline = touchRead(TOUCH_PAD_LEFT);
    touchPadCenterBaseline = touchRead(TOUCH_PAD_CENTER);
    touchPadRightBaseline = touchRead(TOUCH_PAD_RIGHT);

    uint16_t left_threshold = touchPadLeftBaseline * TOUCH_THRESHOLD;
    uint16_t center_threshold = touchPadCenterBaseline * TOUCH_THRESHOLD;
    uint16_t right_threshold = touchPadRightBaseline * TOUCH_THRESHOLD;

    // Re-enable interrupts before delay() calls (delay() requires interrupts on ESP32)
    interrupts();

    // Serial.printf("Touch sensor thresholds - Left: %d, Center: %d, Right: %d\n",
    //               left_threshold, center_threshold, right_threshold);

    // Wait for touch readings to stabilize (only at startup)
    // Use the same percentage-based deviation check as used elsewhere in the codebase
    // Note: This happens with interrupts enabled but touch interrupts are detached, so safe
    if (checkStability) {
        uint16_t currentLeft, currentCenter, currentRight;
        int stabilityCount = 0;
        const int requiredStableReadings = 3;
        const int maxStabilityAttempts = 50;  // Prevent infinite loop
        int attempts = 0;
        
        while (stabilityCount < requiredStableReadings && attempts < maxStabilityAttempts) {
            currentLeft = touchRead(TOUCH_PAD_LEFT);
            currentCenter = touchRead(TOUCH_PAD_CENTER);
            currentRight = touchRead(TOUCH_PAD_RIGHT);
            
            // Check all pads have deviation below threshold (not being touched)
            // Using same logic as startSleep(): abs(current / baseline - 1.0) < TOUCH_THRESHOLD
            float leftDev = abs((float)currentLeft / touchPadLeftBaseline - 1.0);
            float centerDev = abs((float)currentCenter / touchPadCenterBaseline - 1.0);
            float rightDev = abs((float)currentRight / touchPadRightBaseline - 1.0);
            
            if (leftDev < TOUCH_THRESHOLD && 
                centerDev < TOUCH_THRESHOLD && 
                rightDev < TOUCH_THRESHOLD) {
                stabilityCount++;
            } else {
                stabilityCount = 0;  // Reset counter if any pad shows touch deviation
            }
            attempts++;
            delay(5);
        }

        // Clear wakePad again after ensuring stable readings
        wakePad = 0;
    }

    // Disable interrupts during interrupt attachment to prevent race conditions
    noInterrupts();

    // Enable wake-up on touch pads
    esp_sleep_enable_touchpad_wakeup();

    // Attach interrupt callbacks - each callback is now correctly named for its physical pad
    touchAttachInterrupt(TOUCH_PAD_LEFT, onLeftPadTouch, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onCenterPadTouch, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onRightPadTouch, right_threshold);

    // Clear wakePad one more time before re-enabling interrupts
    wakePad = 0;

    // Record calibration timestamp (in microseconds) before re-enabling interrupts
    // This prevents false triggers immediately after calibration completes
    // ISRs will ignore triggers within 10ms (10000 microseconds) of this timestamp
    lastCalibrationTime_us = esp_timer_get_time();

    // Re-enable interrupts after calibration is complete
    interrupts();
    
    // Small delay after re-enabling interrupts to allow any hardware settling
    delay(10);
    
    // Final clear of wakePad after interrupts are enabled and settled
    wakePad = 0;
}

/**
 * Interprets which touch sensor was activated after waking from sleep
 * Sets the appropriate touch flag (leftTouch, centerTouch, rightTouch) 
 * Increments the corresponding counter
 * 
 * Note: Physical pad to touch mapping is rotated:
 * - LEFT pad (wakePad=1) → centerTouch
 * - CENTER pad (wakePad=2) → rightTouch
 * - RIGHT pad (wakePad=3) → leftTouch
 * 
 * The hardware interrupt has already verified the touch is above threshold,
 * so we trust the wakePad value without re-checking (which could fail if
 * the touch value has decayed by interpretation time).
 */
void FED4::interpretTouch()
{
    // Map wakePad values to touch flags (compensating for rotated mapping)
    // Hardware interrupt already validated threshold, so we trust wakePad value
    if (wakePad == 1) {  // LEFT pad → triggers centerTouch
        centerCount++;
        centerTouch = true;
    } else if (wakePad == 2) {  // CENTER pad → triggers rightTouch
        rightCount++;
        rightTouch = true;
    } else if (wakePad == 3) {  // RIGHT pad → triggers leftTouch
        leftCount++;
        leftTouch = true;
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

