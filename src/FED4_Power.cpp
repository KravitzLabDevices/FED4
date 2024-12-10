#include "FED4.h"

void FED4::enterLightSleep()
{
    // Power down
    Serial.println("Powering down...");
    noPix();
    // !! let's not power down until everything works
    // digitalWrite(LDO2_ENABLE, LOW);
    // digitalWrite(FRONT_RGB_LED, LOW);

    // Enter sleep
    Serial.println("Entering light sleep...");
    delay(50);

    esp_light_sleep_start();

    // Clear any pending touch status
    touch_pad_clear_status();

    // Now interpret the touch that woke us
    interpretTouch();

    Serial.println("Woke up!");
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);
    pinMode(FRONT_RGB_LED, OUTPUT);
    digitalWrite(FRONT_RGB_LED, HIGH);

    purplePix();
    serialStatusReport();
    // calibrateTouchSensors();
}