#include "FED4.h"
#include "driver/touch_pad.h"

void IRAM_ATTR FED4::onTouchWakeUp()
{
    // empty
}

void FED4::initializeTouch()
{
    // Initialize touch pad peripheral
    touch_pad_init();

    // Configure touch pads
    touch_pad_config(TOUCH_PAD_LEFT);
    touch_pad_config(TOUCH_PAD_CENTER);
    touch_pad_config(TOUCH_PAD_RIGHT);

    // Rest of your existing configuration...
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

    // // Set measurement time and sleep time
    // touch_pad_set_meas_time(0x1000, 0xffff);

    // // Set filter config with updated struct name
    // touch_filter_config_t filter_info = {
    //     .mode = TOUCH_PAD_FILTER_IIR_16,
    //     .debounce_cnt = 1, // 1 time count
    //     .noise_thr = 0,    // 50%
    //     .jitter_step = 4,  // use for jitter mode
    //     .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    // };
    // touch_pad_filter_set_config(&filter_info);

    // // Denoise setting
    // touch_pad_denoise_t denoise = {
    //     .grade = TOUCH_PAD_DENOISE_BIT4,
    //     .cap_level = TOUCH_PAD_DENOISE_CAP_L4};
    // touch_pad_denoise_set_config(&denoise);
    // touch_pad_denoise_enable();

    delay(200);
}

void FED4::calibrateTouchSensors()
{
    Serial.println("Touch sensor calibration");

    touchPadLeftBaseline = touchRead(TOUCH_PAD_LEFT);
    touchPadCenterBaseline = touchRead(TOUCH_PAD_CENTER);
    touchPadRightBaseline = touchRead(TOUCH_PAD_RIGHT);

    uint16_t left_threshold = touchPadLeftBaseline * TOUCH_THRESHOLD;
    uint16_t center_threshold = touchPadCenterBaseline * TOUCH_THRESHOLD;
    uint16_t right_threshold = touchPadRightBaseline * TOUCH_THRESHOLD;

    Serial.printf("Thresholds - Left: %d, Center: %d, Right: %d\n",
                  left_threshold, center_threshold, right_threshold);

    // Enable wake-up on touch pads
    esp_sleep_enable_touchpad_wakeup();

    // Set individual thresholds for each pad
    touchAttachInterrupt(TOUCH_PAD_LEFT, onTouchWakeUp, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onTouchWakeUp, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onTouchWakeUp, right_threshold);
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