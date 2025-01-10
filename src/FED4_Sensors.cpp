#include "FED4.h"
#include "driver/touch_pad.h"

// Static flags to communicate between ISR and main code
static volatile bool leftTouchFlag = false;
static volatile bool centerTouchFlag = false;
static volatile bool rightTouchFlag = false;

/**
 * By using IRAM_ATTR, we ensure:
 * - The ISR code is loaded into ESP32's internal RAM (IRAM)
 * - The function can execute immediately when triggered
 * - No potential delays from flash memory access
 */

// Add IRAM_ATTR to ensure ISR runs from RAM
static void IRAM_ATTR onLeftTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        leftTouchFlag = true;
        lastTouchTime = currentTime;
    }
}

// Same for other touch handlers
static void IRAM_ATTR onRightTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        rightTouchFlag = true;
        lastTouchTime = currentTime;
    }
}

static void IRAM_ATTR onCenterTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        centerTouchFlag = true;
        lastTouchTime = currentTime;
    }
}

bool FED4::initializeTouch()
{
    Serial.println("Enabling touchpad wakeup...");

    // Initialize touch pad peripheral
    esp_err_t err = touch_pad_init();
    if (err != ESP_OK)
    {
        Serial.println("Touch pad init failed");
        return false;
    }

    // Set touch pad interrupt threshold
    err = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    if (err != ESP_OK)
    {
        Serial.println("Touch pad voltage config failed");
        return false;
    }

    // Enable wake-up on touch pads
    esp_sleep_enable_touchpad_wakeup();

    delay(200); // Wait for touch pad system to stabilize
    return true;
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
    touchAttachInterrupt(TOUCH_PAD_LEFT, ::onLeftTouch, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, ::onCenterTouch, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, ::onRightTouch, right_threshold);
}

void FED4::interpretTouch()
{
    // are we positive this doesn't work?
    touch_pad_t wakePad = esp_sleep_get_touchpad_wakeup_status();
    Serial.printf("Wake pad: %d\n", wakePad);

    // Reset all touch states
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Check flags and handle touches
    if (leftTouchFlag)
    {
        leftTouch = true;
        leftCount++;
        logData("Left");
        greenPix();
        outputPulse(1, 100);
        leftTouchFlag = false;
        updateDisplay();
    }

    if (centerTouchFlag)
    {
        centerTouch = true;
        centerCount++;
        logData("Center");
        bluePix();
        outputPulse(2, 100);
        centerTouchFlag = false;
        updateDisplay();
    }

    if (rightTouchFlag)
    {
        rightTouch = true;
        rightCount++;
        logData("Right");
        redPix();
        outputPulse(3, 100);
        rightTouchFlag = false;
        updateDisplay();
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