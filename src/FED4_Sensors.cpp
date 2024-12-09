#include "FED4.h"
#include "driver/touch_pad.h"

static FED4 *FED4Instance = nullptr;

// this is the accepted method for callbacks in ESP32, however
// it is not consistent with the highest valued sensor, so we fix it below
// !! we could rever to a single callback, but maybe this ends up working one day
void IRAM_ATTR FED4::onLeftWakeUp()
{
    if (FED4Instance)
    {
        FED4Instance->touchTriggers |= LEFT_TRIGGER;
        FED4Instance->lastTouchValue = touchRead(TOUCH_PAD_LEFT); // Store touch value
    }
}

void IRAM_ATTR FED4::onCenterWakeUp()
{
    if (FED4Instance)
    {
        FED4Instance->touchTriggers |= CENTER_TRIGGER;
        FED4Instance->lastTouchValue = touchRead(TOUCH_PAD_CENTER);
    }
}

void IRAM_ATTR FED4::onRightWakeUp()
{
    if (FED4Instance)
    {
        FED4Instance->touchTriggers |= RIGHT_TRIGGER;
        FED4Instance->lastTouchValue = touchRead(TOUCH_PAD_RIGHT);
    }
}

void FED4::touchPadInit()
{
    FED4Instance = this; // for the IRAM_ATTR callbacks

    // Initialize touch pad peripheral
    touch_pad_init();

    // Configure touch pads
    touch_pad_config(TOUCH_PAD_LEFT);
    touch_pad_config(TOUCH_PAD_CENTER);
    touch_pad_config(TOUCH_PAD_RIGHT);

    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

    // Set measurement time and sleep time
    touch_pad_set_meas_time(0x1000, 0xffff);

    // Set filter config with updated struct name
    touch_filter_config_t filter_info = {
        .mode = TOUCH_PAD_FILTER_IIR_16,
        .debounce_cnt = 1, // 1 time count
        .noise_thr = 0,    // 50%
        .jitter_step = 4,  // use for jitter mode
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    touch_pad_filter_set_config(&filter_info);

    // Denoise setting
    touch_pad_denoise_t denoise = {
        .grade = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4};
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();

    delay(200);
}

void FED4::calibrateTouchSensors()
{
    Serial.println("Touch sensor calibration");

    // Use touchRead instead of touch_pad_read
    baselineTouchSensors();

    uint16_t left_threshold = touchPadLeftBaseline * threshold;
    uint16_t center_threshold = touchPadCenterBaseline * threshold;
    uint16_t right_threshold = touchPadRightBaseline * threshold;

    Serial.printf("Thresholds - Left: %d, Center: %d, Right: %d\n",
                  left_threshold, center_threshold, right_threshold);

    // Set individual thresholds for each pad
    touchAttachInterrupt(TOUCH_PAD_LEFT, onLeftWakeUp, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onCenterWakeUp, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onRightWakeUp, right_threshold);

    esp_sleep_enable_touchpad_wakeup();
}

// !!let's consider if this is going to be useful or lead to more bugs mid-session
void FED4::baselineTouchSensors()
{
    Serial.println("Re-baselining...");
    touchPadLeftBaseline = touchRead(TOUCH_PAD_LEFT);
    touchPadCenterBaseline = touchRead(TOUCH_PAD_CENTER);
    touchPadRightBaseline = touchRead(TOUCH_PAD_RIGHT);
    Serial.println("*****");
    Serial.printf("Pin LEFT: %d\n", touchPadLeftBaseline);
    Serial.printf("Pin CENTER: %d\n", touchPadCenterBaseline);
    Serial.printf("Pin RIGHT: %d\n", touchPadRightBaseline);
    Serial.println("*****");
}

void FED4::interpretTouch()
{
    // Read current values
    uint16_t leftVal = touchRead(TOUCH_PAD_LEFT);
    uint16_t centerVal = touchRead(TOUCH_PAD_CENTER);
    uint16_t rightVal = touchRead(TOUCH_PAD_RIGHT);

    Serial.printf("Touch values - Left: %d/%d, Center: %d/%d, Right: %d/%d\n",
                  leftVal, touchPadLeftBaseline,
                  centerVal, touchPadCenterBaseline,
                  rightVal, touchPadRightBaseline);

    // Find which sensor had the largest absolute deviation from baseline
    float leftDev = abs((float)leftVal / touchPadLeftBaseline - 1.0);
    float centerDev = abs((float)centerVal / touchPadCenterBaseline - 1.0);
    float rightDev = abs((float)rightVal / touchPadRightBaseline - 1.0);

    // Always count a touch - just determine which one had the largest deviation
    if (leftDev >= centerDev && leftDev >= rightDev)
    {
        Serial.println("Left Poke detected.");
        leftCount++;
        greenPix();
        feedReady = true;
    }
    else if (centerDev >= leftDev && centerDev >= rightDev)
    {
        Serial.println("Center Poke detected.");
        centerCount++;
        bluePix();
    }
    else
    {
        Serial.println("Right Poke detected.");
        rightCount++;
        redPix();
    }
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