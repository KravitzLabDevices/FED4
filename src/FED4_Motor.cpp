#include "FED4.h"

void FED4::feed()
{
    if (feedReady)
    {
        pg1Read = mcp.digitalRead(EXP_PHOTOGATE_1);

        Serial.println("Feeding!");
        while (pg1Read == 1)
        {                     // while pellet well is empty
            stepper.step(-2); // small movement
            delay(10);
            pg1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
            pelletReady = true;
        }

        if (pelletReady)
        {
            pelletCount++;
            pelletReady = false;
        }
        feedReady = false;

        releaseMotor();
        serialStatusReport();
        strcpy(event, "PelletDrop");

        // Monitor retrieval
        unsigned long pelletDrop = millis();
        while (pg1Read == 0)
        { // while pellet well is full
            bluePix();
            pg1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
            retrievalTime = millis() - pelletDrop;
            if (retrievalTime > 10000)
                break;
        }
        redPix();
        strcpy(event, "PelletTaken");
        retrievalTime = 0;
    }

    updateDisplay();

    // Rebaseline touch sensors every 5 pellets
    if (pelletCount % 5 == 0 && pelletCount > 1)
    {
        baselineTouchSensors();
    }
}

void FED4::checkForPellet()
{
    pg1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
}

void FED4::releaseMotor()
{
    digitalWrite(MOTOR_PIN_1, LOW);
    digitalWrite(MOTOR_PIN_2, LOW);
    digitalWrite(MOTOR_PIN_3, LOW);
    digitalWrite(MOTOR_PIN_4, LOW);
}

void FED4::vibrate()
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    int wait = 100; // !! added, can't find wait var anywhere???
    delay(wait);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}