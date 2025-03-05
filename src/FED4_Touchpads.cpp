#include "FED4.h"
#include "driver/touch_pad.h"

void IRAM_ATTR FED4::onTouchWakeUp()
{
    // empty
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
    delay(200);
    return true;
}

void FED4::calibrateTouchSensors()
{
    touchPadLeftBaseline = touchRead(TOUCH_PAD_LEFT);
    touchPadCenterBaseline = touchRead(TOUCH_PAD_CENTER);
    touchPadRightBaseline = touchRead(TOUCH_PAD_RIGHT);

    uint16_t left_threshold = touchPadLeftBaseline * TOUCH_THRESHOLD;
    uint16_t center_threshold = touchPadCenterBaseline * TOUCH_THRESHOLD;
    uint16_t right_threshold = touchPadRightBaseline * TOUCH_THRESHOLD;

    Serial.printf("Touch sensor thresholds - Left: %d, Center: %d, Right: %d\n",
                  left_threshold, center_threshold, right_threshold);

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
    // Read current values
    uint16_t leftVal = touchRead(TOUCH_PAD_LEFT);
    uint16_t centerVal = touchRead(TOUCH_PAD_CENTER);
    uint16_t rightVal = touchRead(TOUCH_PAD_RIGHT);

    // Reset all touch states, they should be cleared elsewhere before this too
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Serial.printf("Touch values - Left: %d/%d, Center: %d/%d, Right: %d/%d\n",
    //               leftVal, touchPadLeftBaseline,
    //               centerVal, touchPadCenterBaseline,
    //               rightVal, touchPadRightBaseline);

    // Find which sensor had the largest absolute deviation from baseline
    float leftDev = abs((float)leftVal / touchPadLeftBaseline - 1.0);
    float centerDev = abs((float)centerVal / touchPadCenterBaseline - 1.0);
    float rightDev = abs((float)rightVal / touchPadRightBaseline - 1.0);

    // Always count a touch - just determine which one had the largest deviation, set others to false
    if (leftDev >= centerDev && leftDev >= rightDev)
    {
        leftCount++;
        greenPix();
        // Serial.println("Left Poke detected.");
        logData("Left"); 
        leftTouch = true;
        outputPulse(1, 10);
        //updateDisplay();  //can't do this here, it's too slow
    }
    else if (centerDev >= leftDev && centerDev >= rightDev)
    {
        centerCount++;
        bluePix();
        // Serial.println("Center Poke detected.");
        logData("Center"); 
        centerTouch = true;
        outputPulse(2, 10);
        //updateDisplay();
    }
    else
    {
        rightCount++;
        redPix();
        // Serial.println("Right Poke detected.");
        logData("Right"); 
        rightTouch = true;
        outputPulse(3, 10);
        //updateDisplay();
    }

    // Clear any pending touch pad interrupts
    touch_pad_clear_status();
}

void FED4::monitorTouchSensors()
{
    Serial.println("Starting touch sensor monitoring...");
    Serial.println("LEFT,CENTER,RIGHT"); // CSV header

    while (digitalRead(BUTTON_2) == HIGH)
    {
        uint16_t leftVal = touchRead(TOUCH_PAD_LEFT);
        uint16_t centerVal = touchRead(TOUCH_PAD_CENTER);
        uint16_t rightVal = touchRead(TOUCH_PAD_RIGHT);

        // Print in CSV format for easy plotting
        Serial.printf("%d,%d,%d\n", leftVal, centerVal, rightVal);

        delay(10);
    }

    Serial.println("Touch sensor monitoring stopped.");
}