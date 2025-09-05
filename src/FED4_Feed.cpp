#include "FED4.h"

/**
 * Feeds the mouse by dispensing a pellet from the hopper
 */
void FED4::feed()
{
    initFeeding();
    dispense(); 
    handlePelletSettling();
    handlePelletInWell();
    finishFeeding();
}

void FED4::initFeeding() {
    pelletPresent = checkForPellet();
    pelletDropped = didPelletDrop();
    pelletReady = false;
    dispenseError = false;
    //lightsOff();
    // Serial.println("Feeding!");
}

void FED4::dispense() {
    while (!pelletPresent && !pelletDropped) // while no pellet is present and none has dropped
    {                     
        redPix();
        // check if pellet has dropped or is present
        pelletDropped = didPelletDrop();
        pelletPresent = checkForPellet();
        pelletReady = true;

        // small motor movement
        stepper.step(-10);
        delay(2);
        motorTurns++;
        // delay for 1s roughly each pellet position
        if (motorTurns % 25 == 0)
        {
            releaseMotor();
            delay(1000);
        }

        //handle jam movements
        handleJams();
    }
}

void FED4::handleJams() {
        // if stepper is called too many times without a dispense do a small movement to remove jam
        if (motorTurns % 100 == 0)
        {
            minorJamClear();
        }

        if (motorTurns % 200 == 0)
        {
            vibrateJamClear();
        }

        if (motorTurns > 2000)  //how many motorTurns before FED4 stops trying and shuts off?  Each full rotation of the hopper is ~1000 motorTurns
        {
            jammed();
    }
}

void FED4::handlePelletSettling() {
        if (pelletReady) {
        // Serial.println("PelletDrop");
        pelletDropTime = millis();
        pelletCount++;
        logData("PelletDrop");
        }        
  
    releaseMotor();

    // Wait up to 500ms for pellet to settle in well if 500ms passes without detection, set dispenseError to true
    unsigned long startWait = millis();
    bool pelletDetected = false;

    while (millis() - startWait < 500) {
        if (checkForPellet()) {
            pelletDetected = true;
            pelletWellTime = millis();
            break; // Exit if pellet is detected
        }
        delay(10);
    }

    // if pellet is not detected, set dispenseError to true
    if (!pelletDetected) {
        dispenseError = true;
    }

    // Calculate time since pellet drop
    retrievalTime = (millis() - pelletWellTime) / 1000.0;
    // Serial.println("Pellet in Well");
}   

void FED4::handlePelletInWell() {
    pelletPresent = checkForPellet();
    updateDisplay();
    while (pelletPresent)
    { // while pellet is in well, monitor for pokes and retrieval time
        bluePix(); 
        pelletPresent = checkForPellet();

        retrievalTime = (static_cast<float>(millis() - pelletWellTime)) / 1000.0f;
        if (retrievalTime > 20)
            break;

        //log pokes while pellet is in well
        // Read current touch values
        uint16_t leftVal = touchRead(TOUCH_PAD_LEFT);
        uint16_t centerVal = touchRead(TOUCH_PAD_CENTER);
        uint16_t rightVal = touchRead(TOUCH_PAD_RIGHT);

        // Check for significant deviations from baseline
        float leftDev = abs((float)leftVal / touchPadLeftBaseline - 1.0);
        float centerDev = abs((float)centerVal / touchPadCenterBaseline - 1.0);
        float rightDev = abs((float)rightVal / touchPadRightBaseline - 1.0);

        // Only log pokes if there's a significant deviation
        if (leftDev >= TOUCH_THRESHOLD) {
            leftCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            // Serial.println("LeftWithPellet");
            logData("LeftWithPellet");
            click();
            updateDisplay();
            outputPulse(1, 100);
            //wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_LEFT) / touchPadLeftBaseline - 1.0) >= TOUCH_THRESHOLD) {
                delay(10);
            }
        }
        else if (centerDev >= TOUCH_THRESHOLD) {
            centerCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            // Serial.println("CenterWithPellet");
            logData("CenterWithPellet"); 
            click();
            updateDisplay();
            bluePix();
            outputPulse(2, 100);
            //wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_CENTER) / touchPadCenterBaseline - 1.0) >= TOUCH_THRESHOLD) {
                delay(10);
            }
        }
        else if (rightDev >= TOUCH_THRESHOLD) {
            rightCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            // Serial.println("RightWithPellet");
            logData("RightWithPellet");
            click();
            updateDisplay();
            redPix();
            outputPulse(3, 100);
            //wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_RIGHT) / touchPadRightBaseline - 1.0) >= TOUCH_THRESHOLD) {
                delay(10);
            }
        }
    }
}

void FED4::finishFeeding() {
        purplePix();
    // Serial.println("Pellet Removed");

    if (pelletReady) {
        if (dispenseError) {
            retrievalTime = 0.0;
            logData("PelletNotDetected");
        } else {
            logData("PelletTaken");
            blockPokeCount = 0;  // Reset block poke count when pellet is taken
        }
    }

    // Reset variables
    pelletReady = false;
    retrievalTime = 0.0;
    dispenseError = false;
    
    // Reset touch states after handling the feed
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Rebaseline touch sensors
    reBaselineTouches = 3;
    if ((leftCount + rightCount + centerCount) % reBaselineTouches == 0 && (leftCount + rightCount + centerCount) > 5)
    {
        calibrateTouchSensors();
    }

}

/** 
 * Checks if the pellet is present in the center port
 * 
 * @return bool - true if pellet is present, false otherwise
 */
bool FED4::checkForPellet()
{
    return !mcp.digitalRead(EXP_PHOTOGATE_1);
}

/** 
 * Checks if the pellet is present dropped 
 * 
 * @return bool - true if pellet dropped, false otherwise
 */
bool FED4::didPelletDrop()
{
    if (dropSensorAvailable) {
        //With drop sensor use:
        return !mcp.digitalRead(EXP_PHOTOGATE_4);
    } else {
        //Without drop sensor use:
        return false; // Always return false when sensor is not available
    }
}

/**
 * Initializes the drop sensor and checks its status
 * 
 * @return bool - true if drop sensor is working (HIGH), false if broken or not present (LOW)
 */
bool FED4::initializeDropSensor()
{
    // Read the drop sensor status
    bool sensorStatus = mcp.digitalRead(EXP_PHOTOGATE_4);
    
    // Set the flag based on sensor status
    dropSensorAvailable = sensorStatus;
    
    // Return true if HIGH (sensor is good), false if LOW (broken or not present)
    return sensorStatus;
}