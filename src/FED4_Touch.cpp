#include "FED4.h"

// Initialize the static members
uint8_t FED4::wakePad = 0;  // 0=none, 1=left, 2=center, 3=right

// Separate ISR callbacks for each touch pad (avoids legacy driver API calls)
void IRAM_ATTR FED4::onTouchWakeUp()
{
    // Generic wake-up handler - wakePad is set by specific handlers below
}

// ISR callbacks for touch pad interrupts - names match the physical pad they're attached to
// Inhibit triggers within 30ms of calibration to prevent false triggers
void IRAM_ATTR onLeftPadTouch() {
    // Ignore triggers within 30ms (30000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 30000) {
        return;  // Too soon after calibration, ignore this trigger
    }
    FED4::wakePad = 1;  // LEFT pad
}

void IRAM_ATTR onCenterPadTouch() {
    // Ignore triggers within 30ms (30000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 30000) {
        return;  // Too soon after calibration, ignore this trigger
    }
    FED4::wakePad = 2;  // CENTER pad
}

void IRAM_ATTR onRightPadTouch() {
        // Ignore triggers within 30ms (30000 microseconds) of calibration
    int64_t now_us = esp_timer_get_time();
    if (now_us - lastCalibrationTime_us < 30000) {
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
    
    delay(5);  // Allow touch subsystem to stabilize
    return true;
}

void FED4::calibrateTouchSensors(bool checkStability)
{
    // Mark calibration time early to suppress any in-flight touches
    lastCalibrationTime_us = esp_timer_get_time();

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

    // Re-enable interrupts before delay() calls (delay() requires interrupts on ESP32)
    interrupts();

    // If we already have baselines, wait briefly for pads to be released/stable
    if (touchPadLeftBaseline > 0 && touchPadCenterBaseline > 0 && touchPadRightBaseline > 0) {
        const int maxReleaseAttempts = 40; // ~200ms max
        int attempts = 0;
        while (attempts < maxReleaseAttempts) {
            float leftDev = abs((float)touchRead(TOUCH_PAD_LEFT) / touchPadLeftBaseline - 1.0);
            float centerDev = abs((float)touchRead(TOUCH_PAD_CENTER) / touchPadCenterBaseline - 1.0);
            float rightDev = abs((float)touchRead(TOUCH_PAD_RIGHT) / touchPadRightBaseline - 1.0);
            if (leftDev < TOUCH_THRESHOLD && centerDev < TOUCH_THRESHOLD && rightDev < TOUCH_THRESHOLD) {
                break;
            }
            attempts++;
            delay(5);
        }
    }

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

    // Re-enable interrupts after calibration is complete
    interrupts();
    
    // Small delay after re-enabling interrupts to allow any hardware settling
    delay(10);
    
    // Final clear of wakePad after interrupts are enabled and settled
    wakePad = 0;
}

/**
 * Interprets which touch sensor was activated after waking from sleep
 * Uses software-based majority vote detection to handle crosstalk
 * Sets the appropriate touch flag (leftTouch, centerTouch, rightTouch) 
 * Increments the corresponding counter
 * 
 * Physical pad to touch mapping (direct):
 * - LEFT pad (TOUCH_PAD_LEFT/TOUCH_PAD_NUM6) → leftTouch
 * - CENTER pad (TOUCH_PAD_CENTER/TOUCH_PAD_NUM5) → centerTouch
 * - RIGHT pad (TOUCH_PAD_RIGHT/TOUCH_PAD_NUM1) → rightTouch
 * 
 * This function ignores which ISR fired and instead takes multiple readings
 * while the touch is held, then determines which pad was actually touched
 * based on majority vote. This handles electrical crosstalk more reliably.
 */
void FED4::interpretTouch()
{
    // Small delay to allow system to stabilize after wake-up
    delay(1);
    
    // Read current touch values (exactly like the example)
    uint16_t valLeft = touchRead(TOUCH_PAD_LEFT);
    uint16_t valCenter = touchRead(TOUCH_PAD_CENTER);
    uint16_t valRight = touchRead(TOUCH_PAD_RIGHT);
    
    // Calculate deviations from baseline (exactly like the example)
    float devLeft = abs((float)valLeft / touchPadLeftBaseline - 1.0);
    float devCenter = abs((float)valCenter / touchPadCenterBaseline - 1.0);
    float devRight = abs((float)valRight / touchPadRightBaseline - 1.0);
    
    // Check if any pad is above threshold (exactly like the example)
    bool touchDetected = (devLeft >= TOUCH_THRESHOLD) || 
                         (devCenter >= TOUCH_THRESHOLD) || 
                         (devRight >= TOUCH_THRESHOLD);
    
    if (touchDetected) {
        // Software-based touch determination: find which pad changed most
        // This works around electrical crosstalk causing wrong interrupts to fire
        float maxDev = max(max(devLeft, devCenter), devRight);
        
        // Determine actual touch based on which pad changed most (software detection)
        uint8_t actualTouch = 0;  // 0=none, 1=LEFT, 2=CENTER, 3=RIGHT
        const char* actualPadName = "NONE";
        
        if (maxDev >= TOUCH_THRESHOLD) {
            if (maxDev == devLeft && devLeft >= TOUCH_THRESHOLD) {
                actualTouch = 1;
                actualPadName = "LEFT";
            } else if (maxDev == devCenter && devCenter >= TOUCH_THRESHOLD) {
                actualTouch = 2;
                actualPadName = "CENTER";
            } else if (maxDev == devRight && devRight >= TOUCH_THRESHOLD) {
                actualTouch = 3;
                actualPadName = "RIGHT";
            }
        }
        
        // Count votes for each pad to determine majority (exactly like the example)
        int leftPadVotes = 0;    // TOUCH_PAD_LEFT (NUM6) → leftTouch
        int centerPadVotes = 0;  // TOUCH_PAD_CENTER (NUM5) → centerTouch
        int rightPadVotes = 0;   // TOUCH_PAD_RIGHT (NUM1) → rightTouch
        int totalReadings = 0;
        
        unsigned long touchStartTime = millis();
        int readingCount = 0;
        
        // Track peak deviations to detect release (handle case where one pad always reads high)
        float peakMaxDev = 0.0;
        float peakDevLeft = 0.0;
        float peakDevCenter = 0.0;
        float peakDevRight = 0.0;
        const unsigned long maxSamplingTime_ms = 2000;  // Max 2 seconds of sampling
        
        // Require multiple consecutive readings below threshold before considering released
        // This handles cases where touch values stabilize but are still being touched
        const int minReleaseReadings = 2;  // Require 2 consecutive readings below threshold (~2ms)
        int belowThresholdCount = 0;
        
        // Continuously read and vote while touch is held (like the example, but with timeout)
        while (touchDetected && (millis() - touchStartTime < maxSamplingTime_ms)) {
            valLeft = touchRead(TOUCH_PAD_LEFT);
            valCenter = touchRead(TOUCH_PAD_CENTER);
            valRight = touchRead(TOUCH_PAD_RIGHT);
            devLeft = abs((float)valLeft / touchPadLeftBaseline - 1.0);
            devCenter = abs((float)valCenter / touchPadCenterBaseline - 1.0);
            devRight = abs((float)valRight / touchPadRightBaseline - 1.0);
            
            // Detect if baselines are clearly wrong (value << baseline or value >> baseline)
            // If baseline is way off (more than 10x difference), exclude from detection
            bool leftBaselineBad = (touchPadLeftBaseline > 0 && valLeft > 0 && 
                                   (touchPadLeftBaseline > valLeft * 10 || valLeft > touchPadLeftBaseline * 10));
            bool centerBaselineBad = (touchPadCenterBaseline > 0 && valCenter > 0 && 
                                     (touchPadCenterBaseline > valCenter * 10 || valCenter > touchPadCenterBaseline * 10));
            bool rightBaselineBad = (touchPadRightBaseline > 0 && valRight > 0 && 
                                    (touchPadRightBaseline > valRight * 10 || valRight > touchPadRightBaseline * 10));
            
            // Calculate valid deviations (exclude pads with bad baselines)
            float validDevLeft = leftBaselineBad ? 0 : devLeft;
            float validDevCenter = centerBaselineBad ? 0 : devCenter;
            float validDevRight = rightBaselineBad ? 0 : devRight;
            
            // Recalculate max deviation from valid pads only
            float maxDev = max(max(validDevLeft, validDevCenter), validDevRight);
            
            // Track peaks (only for valid pads)
            if (!leftBaselineBad && devLeft > peakDevLeft) peakDevLeft = devLeft;
            if (!centerBaselineBad && devCenter > peakDevCenter) peakDevCenter = devCenter;
            if (!rightBaselineBad && devRight > peakDevRight) peakDevRight = devRight;
            
            if (maxDev > peakMaxDev) {
                peakMaxDev = maxDev;
            }
            
            // Recalculate touchDetected each iteration (exactly like the example)
            // But exclude pads with bad baselines from the detection check
            bool currentTouchDetected = (!leftBaselineBad && devLeft >= TOUCH_THRESHOLD) || 
                                       (!centerBaselineBad && devCenter >= TOUCH_THRESHOLD) || 
                                       (!rightBaselineBad && devRight >= TOUCH_THRESHOLD);
            
            if (currentTouchDetected) {
                belowThresholdCount = 0;  // Reset counter when touch is detected
                touchDetected = true;
            } else {
                // Touch below threshold - count consecutive readings
                belowThresholdCount++;
                // Require multiple consecutive readings below threshold before considering released
                // This prevents premature release when touch values stabilize but are still touching
                if (belowThresholdCount >= minReleaseReadings) {
                    touchDetected = false;
                } else {
                    touchDetected = true;  // Still consider touched until we have enough readings
                }
            }
            
            uint8_t currentActualTouch = 0;
            const char* currentPadName = "NONE";
            
            if (maxDev >= TOUCH_THRESHOLD) {
                
                // Calculate valid deviations (exclude pads with bad baselines)
                float validDevLeft = leftBaselineBad ? 0 : devLeft;
                float validDevCenter = centerBaselineBad ? 0 : devCenter;
                float validDevRight = rightBaselineBad ? 0 : devRight;
                
                // Recalculate maxDev from only valid pads
                float validMaxDev = max(max(validDevLeft, validDevCenter), validDevRight);
                
                // Only vote if we have a valid pad above threshold
                if (validMaxDev >= TOUCH_THRESHOLD) {
                    // Use standard deviation-based voting (only for pads with good baselines)
                    if (validMaxDev == validDevLeft && validDevLeft >= TOUCH_THRESHOLD) {
                        currentActualTouch = 1;
                        currentPadName = "LEFT";
                        leftPadVotes++;
                        totalReadings++;
                    } else if (validMaxDev == validDevCenter && validDevCenter >= TOUCH_THRESHOLD) {
                        currentActualTouch = 2;
                        currentPadName = "CENTER";
                        centerPadVotes++;
                        totalReadings++;
                    } else if (validMaxDev == validDevRight && validDevRight >= TOUCH_THRESHOLD) {
                        currentActualTouch = 3;
                        currentPadName = "RIGHT";
                        rightPadVotes++;
                        totalReadings++;
                    }
                }
            }
            
            delay(1);
        }
        
        // Clear all state after touch is released (exactly like the example)
        // Reset interrupt flag
        wakePad = 0;
        
        // Take fresh readings to clear any stale values (reduced from 5 to 3 for speed)
        for (int i = 0; i < 3; i++) {
            touchRead(TOUCH_PAD_LEFT);   // Read to clear internal state
            touchRead(TOUCH_PAD_CENTER);
            touchRead(TOUCH_PAD_RIGHT);
            delayMicroseconds(500);  // Use microseconds for faster clearing
        }
        
        // Calculate total touch duration
        unsigned long touchDuration = millis() - touchStartTime;
        pokeDuration = (float)touchDuration;  // Store in class member for logging
        
        // Determine majority pad based on votes
        uint8_t detectedPad = 0;  // 0=none, 1=left, 2=center, 3=right
        const char* padName = "NONE";
        
        if (totalReadings > 0) {
            if (leftPadVotes >= centerPadVotes && leftPadVotes >= rightPadVotes && leftPadVotes > 0) {
                detectedPad = 1;  // LEFT pad
                padName = "LEFT";
            } else if (centerPadVotes >= rightPadVotes && centerPadVotes > 0) {
                detectedPad = 2;  // CENTER pad
                padName = "CENTER";
            } else if (rightPadVotes > 0) {
                detectedPad = 3;  // RIGHT pad
                padName = "RIGHT";
            }
        }
        
        // Map detected pad to touch flags (direct mapping)
        if (detectedPad == 1) {  // LEFT pad → triggers leftTouch
            leftCount++;
            leftTouch = true;
        } else if (detectedPad == 2) {  // CENTER pad → triggers centerTouch
            centerCount++;
            centerTouch = true;
        } else if (detectedPad == 3) {  // RIGHT pad → triggers rightTouch
            rightCount++;
            rightTouch = true;
        }
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

