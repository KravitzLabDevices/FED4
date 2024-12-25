#include "FED4.h"

void FED4::sleep()
{
    // Enter sleep
    Serial.println("Entering light sleep...");
    Serial.flush();

    // SD.end();
    // pinMode(SD_CS, INPUT); // Release CS pin to high-impedance state

    LDO2_OFF();

    esp_light_sleep_start();

    interpretTouch(); // do first to capture accurate touch values

    // pinMode(SD_CS, OUTPUT);
    // digitalWrite(SD_CS, HIGH); // deselect SD card

    LDO2_ON();
    // initializeSD(); // Initialize SD after dummy clocks

    logData(); // event set in interpretTouch() but we need SD card online to log

    // display still works suggesting its not SPI, but the SD card

    Serial.println("Woke up!");
    purplePix();
    serialStatusReport();
}

bool FED4::initializeLDOs()
{
    pinMode(LDO2_ENABLE, OUTPUT);
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO2_ON();
    LDO3_ON();
    return true;
}

void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
}

void FED4::LDO2_OFF()
{
    digitalWrite(LDO2_ENABLE, LOW);
}

void FED4::LDO3_ON()
{
    mcp.digitalWrite(EXP_LDO3, HIGH);
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
}

void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
}