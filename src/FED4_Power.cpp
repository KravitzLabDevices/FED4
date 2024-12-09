#include "FED4.h"

void FED4::enterLightSleep()
{
    // Power down
    Serial.println("Powering down...");
    noPix();
    digitalWrite(LDO2_ENABLE, LOW);
    digitalWrite(FRONT_RGB_LED, LOW);

    // Enter sleep
    Serial.println("Entering light sleep...");
    delay(50);
    esp_light_sleep_start();

    // Wake up
    Serial.println("Woke up!");
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);
    pinMode(FRONT_RGB_LED, OUTPUT);
    digitalWrite(FRONT_RGB_LED, HIGH);

    // Post-wake actions
    interpretTouch();
    purplePix();
    calibrateTouchSensors();
}