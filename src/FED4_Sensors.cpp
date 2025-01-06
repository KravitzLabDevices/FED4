#include "FED4.h"
#include "driver/touch_pad.h"

// Static flags to communicate between ISR and main code
static volatile bool leftTouchFlag = false;
static volatile bool centerTouchFlag = false;
static volatile bool rightTouchFlag = false;

void FED4::onLeftTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        Serial.println("Left touch detected");
        leftTouchFlag = true; // Set the flag
        lastTouchTime = currentTime;
    }
}

void FED4::onRightTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        Serial.println("Right touch detected");
        rightTouchFlag = true; // Set the flag
        lastTouchTime = currentTime;
    }
}

void FED4::onCenterTouch()
{
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= 100)
    { // Debounce
        Serial.println("Center touch detected");
        centerTouchFlag = true; // Set the flag
        lastTouchTime = currentTime;
    }
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
    touchAttachInterrupt(TOUCH_PAD_LEFT, onLeftTouch, left_threshold);
    touchAttachInterrupt(TOUCH_PAD_CENTER, onCenterTouch, center_threshold);
    touchAttachInterrupt(TOUCH_PAD_RIGHT, onRightTouch, right_threshold);
}

void FED4::interpretTouch()
{
    // Reset all touch states
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Check flags and update class members
    if (leftTouchFlag)
    {
        leftTouch = true;
        leftCount++;
        logData("Left");
        greenPix();
        outputPulse(1, 100);
        leftTouchFlag = false; // Clear the flag
        updateDisplay();
    }

    if (centerTouchFlag)
    {
        centerTouch = true;
        centerCount++;
        logData("Center");
        bluePix();
        outputPulse(2, 100);
        centerTouchFlag = false; // Clear the flag
        updateDisplay();
    }

    if (rightTouchFlag)
    {
        rightTouch = true;
        rightCount++;
        logData("Right");
        redPix();
        outputPulse(3, 100);
        rightTouchFlag = false; // Clear the flag
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