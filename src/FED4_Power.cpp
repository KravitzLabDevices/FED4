#include "FED4.h"

void FED4::enterLightSleep()
{
    // Power down
    Serial.println("Powering down...");
    NoPix();
    digitalWrite(POWER_PIN_1, LOW);
    digitalWrite(POWER_PIN_2, LOW);

    // Enter sleep
    Serial.println("Entering light sleep...");
    delay(50);
    esp_light_sleep_start();

    // Wake up
    Serial.println("Woke up!");
    pinMode(POWER_PIN_1, OUTPUT);
    digitalWrite(POWER_PIN_1, HIGH);
    pinMode(POWER_PIN_2, OUTPUT);
    digitalWrite(POWER_PIN_2, HIGH);

    // Post-wake actions
    interpretTouch();
    PurplePix();
    CalibrateTouchSensors();
}