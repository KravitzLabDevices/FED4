#include "FED4.h"

void FED4::enterLightSleep()
{
    // Power down
    Serial.println("Powering down...");
    noPix();

    // Safely disconnect SD card before power down
    // SPI.end(); // Release SPI pins
    // pinMode(SPI_SCK, INPUT);
    // pinMode(SPI_MOSI, INPUT);
    // pinMode(SPI_MISO, INPUT);
    pinMode(SD_CS, INPUT); // Release CS pin

    LDO2_OFF();

    noPix();

    // Enter sleep
    Serial.println("Entering light sleep...");
    delay(50);

    esp_light_sleep_start();
    digitalWrite(USER_PIN_18, HIGH); // pulse ON

    // Clear any pending touch status
    touch_pad_clear_status();

    // Now interpret the touch that woke us
    interpretTouch();

    Serial.println("Woke up!");
    LDO2_ON();
    digitalWrite(RGB_STRIP_PIN, HIGH);

    // Reinitialize SD card
    initializeSD(); // Re-initialize SPI and SD card

    purplePix();
    serialStatusReport();

    digitalWrite(USER_PIN_18, LOW); // pulse OFF
}

void FED4::initializeLDOs()
{
    pinMode(LDO2_ENABLE, OUTPUT);
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO2_ON();
    LDO3_ON();
}

void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
}

void FED4::LDO2_OFF()
{
    digitalWrite(LDO2_ENABLE, LOW);
}

void FED4::LDO3_ON()
{
    mcp.digitalWrite(EXP_LDO3, HIGH);
}

void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
}