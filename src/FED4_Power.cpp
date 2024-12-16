#include "FED4.h"

void FED4::enterLightSleep()
{
    pinMode(SD_CS, INPUT); // Release CS pin to high-impedance state
    noPix();
//     LDO2_OFF();

    // Enter sleep
    Serial.println("Entering light sleep...");
    Serial.flush();


    esp_light_sleep_start();

    Serial.println("Woke up!");
    
//     LDO2_ON();
    initializeSD();

    interpretTouch();
    purplePix();
    serialStatusReport();
}

void FED4::initializeLDOs()
{
    pinMode(LDO2_ENABLE, OUTPUT);
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO2_ON();
    //     LDO3_ON();  //LDO3 is used to control user settable 3V output
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

// SD on wakeup trials
// void FED4::enterLightSleep()
// {
//     // Power down
//     Serial.println("Powering down...");
//     noPix();

//     Serial.println("Closing SD card...");
//     // SD.end(); // crashes with this

//     Serial.println("Closing SPI communication...");
//     // Then end SPI communication
//     SPI.end();

//     // Then set pins to known states if really needed
//     pinMode(SCK, OUTPUT);
//     pinMode(MOSI, OUTPUT);
//     pinMode(MISO, INPUT);
//     digitalWrite(SCK, LOW);
//     digitalWrite(MOSI, LOW);

//     // Handle CS pin last
//     digitalWrite(SD_CS, LOW);
//     pinMode(SD_CS, INPUT); // Release CS pin to high-impedance state

//     LDO2_OFF();

//     noPix();

//     // Enter sleep
//     Serial.println("Entering light sleep...");
//     delay(50);

//     esp_light_sleep_start();

//     Serial.println("Woke up!");
//     LDO2_ON();
//     delay(100);  // Let LDO2 stabilize
//     SPI.begin(); // Initialize SPI first
//     delay(100);  // Let SPI stabilize
//     pinMode(SD_CS, OUTPUT);
//     digitalWrite(SD_CS, HIGH);
//     delay(100); // Give SD card time to recognize CS

//     // Send dummy clock cycles with CS high to reset SD card
//     for (int i = 0; i < 10; i++)
//     {
//         SPI.transfer(0xFF);
//     }

//     initializeSD();

//     // Now interpret the touch that woke us
//     interpretTouch();
//     purplePix();
//     serialStatusReport();
// }