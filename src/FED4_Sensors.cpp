#include "FED4.h"

void IRAM_ATTR FED4::onWakeUp()
{
    // Empty function to serve as interrupt handler for touchpad wake-up
}

void FED4::touch_pad_init()
{
    touch_pad_init();
    touch_pad_config(TOUCH_PIN_1);
    touch_pad_config(TOUCH_PIN_5);
    touch_pad_config(TOUCH_PIN_6);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // touch_pad_filter_start(10); // not found during compilation
    delay(200);
}

void FED4::CalibrateTouchSensors()
{
    Serial.println("Touch sensor calibration");
    touchAttachInterrupt(TOUCH_PIN_1, onWakeUp, threshold);
    touchAttachInterrupt(TOUCH_PIN_5, onWakeUp, threshold);
    touchAttachInterrupt(TOUCH_PIN_6, onWakeUp, threshold);
    esp_sleep_enable_touchpad_wakeup();
}

void FED4::BaselineTouchSensors()
{
    Serial.println("Re-baselining...");
    baseline1 = touchRead(TOUCH_PIN_1);
    baseline5 = touchRead(TOUCH_PIN_5);
    baseline6 = touchRead(TOUCH_PIN_6);
    Serial.println("*****");
    Serial.printf("Pin 1: %d\n", baseline1);
    Serial.printf("Pin 5: %d\n", baseline5);
    Serial.printf("Pin 6: %d\n", baseline6);
    Serial.println("*****");
}

void FED4::interpretTouch()
{
    int reading1 = abs((int)(touchRead(TOUCH_PIN_1) - baseline1));
    int reading5 = abs((int)(touchRead(TOUCH_PIN_5) - baseline5));
    int reading6 = abs((int)(touchRead(TOUCH_PIN_6) - baseline6));

    Serial.println("Touch readings (baseline subtracted, abs):");
    Serial.printf("Pin 1: %d\n", reading1);
    Serial.printf("Pin 5: %d\n", reading5);
    Serial.printf("Pin 6: %d\n", reading6);

    int maxReading = 0;
    int triggeredPin = -1;

    if (reading1 > threshold && reading1 > maxReading)
    {
        maxReading = reading1;
        triggeredPin = 1;
    }
    if (reading5 > threshold && reading5 > maxReading)
    {
        maxReading = reading5;
        triggeredPin = 5;
    }
    if (reading6 > threshold && reading6 > maxReading)
    {
        maxReading = reading6;
        triggeredPin = 6;
    }

    switch (triggeredPin)
    {
    case 1:
        Serial.println("Wake-up triggered by Right Poke.");
        RightCount++;
        RedPix();
        break;
    case 5:
        Serial.println("Wake-up triggered by Center Poke.");
        CenterCount++;
        BluePix();
        break;
    case 6:
        Serial.println("Wake-up triggered by Left Poke.");
        GreenPix();
        LeftCount++;
        FeedReady = true;
        break;
    default:
        Serial.println("Wake-up reason unclear, taking no action.");
        WakeCount++;
    }
}