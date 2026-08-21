#include "FED4.h"

/**
 * Dispense + settle + awake well monitor (≤20 s for precise retrievalTime).
 * If the pellet is still in the well after that window, pendingRetrieval is set;
 * waitUntil() calls checkLateRetrieval() on the next wake (PSV2 off in light sleep;
 * LatePelletTaken is coarse — up to the UI interval). DispenseError only from jammed().
 */
void FED4::feed()
{
    checkLateRetrieval(); // prior pending take may have happened during sleep
    initFeeding();
    dispense();
    handlePelletSettling();
    handlePelletInWell();
    finishFeeding();
}

void FED4::initFeeding()
{
    pelletPresent = checkForPellet();
    // pelletDropped = didPelletDrop();
    pelletReady = false;
    dispenseError = false;
    lightsOff();
    Serial.println("Feeding!");
}

void FED4::dispense()
{
    while (!pelletPresent && !dispenseError)
    {
        redPix();
        // pelletDropped = didPelletDrop();
        pelletPresent = checkForPellet();

        // Button 1: fake pelletPresent to exit dispense (lab/debug)
        if (digitalRead(BUTTON_1) == 1)
        {
            hapticDoubleBuzz();
            marioPipe();
            pelletPresent = true;
        }

        // if (pelletDropped)
        // {
        //     blockPelletCount++;
        // }

        stepper.step(-10);
        delay(2);
        motorTurns++;
        if (motorTurns % 25 == 0)
        {
            Serial.print("Dispensing... ");
            Serial.println(motorTurns / 25);
            releaseMotor();
            delay(1000);
        }

        handleJams(); // minor/vibrate clears — DispenseError only in jammed()
    }

    if (!dispenseError && pelletPresent)
    {
        pelletReady = true;
    }
}

void FED4::handleJams()
{
    if (motorTurns % 100 == 0)
    {
        minorJamClear();
    }

    if (motorTurns % 200 == 0)
    {
        vibrateJamClear();
    }

    // Hard give-up only — not while jam-clear motor moves are still the strategy
    if (motorTurns > 2000)
    {
        jammed();
    }
}

void FED4::handlePelletSettling()
{
    releaseMotor();

    if (!pelletReady)
    {
        return;
    }

    pelletDropTime = millis();
    pelletCount++;
    logData("PelletDrop");

    unsigned long startWait = millis();
    bool pelletDetected = false;

    while (millis() - startWait < 500)
    {
        if (checkForPellet())
        {
            pelletDetected = true;
            pelletWellTime = millis();
            break;
        }
        delay(10);
    }

    if (!pelletDetected)
    {
        dispenseError = true;
    }
}

void FED4::monitorPelletInWell(uint32_t retrievalTimeoutSec)
{
    pelletPresent = checkForPellet();
    updateDisplay();
    wakePad = 0;

    while (pelletPresent)
    {
        redPix();
        pelletPresent = checkForPellet();

        retrievalTime = (static_cast<float>(millis() - pelletWellTime)) / 1000.0f;
        if (retrievalTime > (float)retrievalTimeoutSec)
        {
            break;
        }

        if (fed4TouchAnyPadActive(TOUCH_THRESHOLD) && capturePoke())
        {
            switch (wakePad)
            {
            case 1:
                logData("LeftWithPellet");
                click();
                updateDisplay();
                outputPulse(1, 100);
                break;
            case 2:
                logData("CenterWithPellet");
                click();
                updateDisplay();
                redPix();
                outputPulse(2, 100);
                break;
            case 3:
                logData("RightWithPellet");
                click();
                updateDisplay();
                redPix();
                outputPulse(2, 100);
                break;
            default:
                break;
            }
            resetTouchFlags();
        }

        delay(10);
    }
}

void FED4::handlePelletInWell()
{
    if (!pelletReady || dispenseError)
    {
        return;
    }
    monitorPelletInWell(20);
}

void FED4::finishFeeding()
{
    redPix();

    if (pelletReady)
    {
        if (dispenseError)
        {
            retrievalTime = 0.0f;
            pendingRetrieval = false;
            pelletPresent = false;
            logData("PelletNotDetected");
            Serial.println("Pellet not detected in well");
        }
        else if (checkForPellet())
        {
            // Awake 20 s window ended; pellet still present — precise time stopped.
            // Next waitUntil wake (timer/touch/button) → checkLateRetrieval().
            pendingRetrieval = true;
            pelletPresent = true;
            Serial.println("Pellet still in well — pending late retrieval");
        }
        else
        {
            pendingRetrieval = false;
            pelletPresent = false;
            logData("PelletTaken");
            blockPokeCount = 0;
            Serial.println("Pellet Removed");
        }
    }

    pelletReady = false;
    if (!pendingRetrieval)
    {
        retrievalTime = 0.0f;
    }

    dispenseError = false;

    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    reBaselineTouches = 3;
    if ((leftCount + rightCount + centerCount) % reBaselineTouches == 0 &&
        (leftCount + rightCount + centerCount) > 5)
    {
        calibrateTouchSensors();
    }
}

/**
 * If feed() left a pellet in the well after the awake retrieval window, and the
 * well is now empty, log LatePelletTaken (approx retrievalTime — may span sleep).
 */
bool FED4::checkLateRetrieval()
{
    if (!pendingRetrieval)
    {
        return false;
    }

    if (checkForPellet())
    {
        return false; // still waiting
    }

    retrievalTime = (static_cast<float>(millis() - pelletWellTime)) / 1000.0f;
    pelletPresent = false; // clear before logData/displayIndicators refresh
    logData("LatePelletTaken");
    blockPokeCount = 0;
    pendingRetrieval = false;
    retrievalTime = 0.0f;
    Serial.println("Late pellet retrieval logged");
    return true;
}

bool FED4::checkForPellet()
{
    return !digitalRead(PHOTOGATE_1);
}

// bool FED4::didPelletDrop()
// {
//     if (dropSensorAvailable)
//     {
//         return !digitalRead(PHOTOGATE_4);
//     }
//     return false;
// }

bool FED4::initializeDropSensor()
{
    bool sensorStatus = digitalRead(PHOTOGATE_4);
    dropSensorAvailable = sensorStatus;
    return sensorStatus;
}
