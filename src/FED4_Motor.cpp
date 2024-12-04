#include "FED4.h"

void FED4::Feed()
{
    if (FeedReady)
    {
        PG1Read = mcp.digitalRead(EXP_PHOTOGATE_1);

        Serial.println("Feeding!");
        while (PG1Read == 1)
        {                     // while pellet well is empty
            stepper.step(-2); // small movement
            delay(10);
            PG1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
            pelletReady = true;
        }

        if (pelletReady)
        {
            PelletCount++;
            pelletReady = false;
        }
        FeedReady = false;

        ReleaseMotor();
        SerialOutput();
        strcpy(Event, "PelletDrop");

        // Monitor retrieval
        unsigned long PelletDrop = millis();
        while (PG1Read == 0)
        { // while pellet well is full
            BluePix();
            PG1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
            RetrievalTime = millis() - PelletDrop;
            if (RetrievalTime > 10000)
                break;
        }
        RedPix();
        strcpy(Event, "PelletTaken");
        RetrievalTime = 0;
    }

    UpdateDisplay();

    // Rebaseline touch sensors every 5 pellets
    if (PelletCount % 5 == 0 && PelletCount > 1)
    {
        BaselineTouchSensors();
    }
}

void FED4::CheckForPellet()
{
    PG1Read = mcp.digitalRead(EXP_PHOTOGATE_1);
}

void FED4::ReleaseMotor()
{
    digitalWrite(MOTOR_PIN_1, LOW);
    digitalWrite(MOTOR_PIN_2, LOW);
    digitalWrite(MOTOR_PIN_3, LOW);
    digitalWrite(MOTOR_PIN_4, LOW);
}

void FED4::Vibrate(unsigned long wait)
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(wait);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}