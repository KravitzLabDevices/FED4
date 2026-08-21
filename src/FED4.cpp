#include "FED4.h"
const char FED4::libraryVer[] = "1.7.0";

/*
 (o)(o)--.
  \../ (  )
  m\/m--m'`--.

This is the library for FED4, an open source device for training mice.
Written by Matt Gaidica and Lex Kravitz
Updated Dec 2024

The FED4 library relies on code from Adafruit and Sparkfun, who invest
significant time and money developing open-source hardware and software.
Please support them!

*/

/********************************************************
 * Constructor
 ********************************************************/

/**
 * Constructor for FED4 class
 */
FED4::FED4() : Adafruit_GFX(DISPLAY_WIDTH, DISPLAY_HEIGHT),
               stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_3, MOTOR_PIN_2, MOTOR_PIN_4)
#ifndef FED4_EXCLUDE_HUBLINK
               ,
               hublink(SD_CS)
#endif
{

    pelletReady = true;
    feedReady = false;
    displayBuffer = nullptr; // Initialize our display buffer pointer
    vcom = false;            // Initialize VCOM state
    vcomLedcActive = false;

    // Initialize counters
    pelletCount = 0;
    centerCount = 0;
    leftCount = 0;
    rightCount = 0;
    wakeCount = 0;
    motorTurns = 0;
    motionCount = 0;
    motionPercentage = 0.0;
    pollCount = 0;

    // Initialize touch states
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Initialize retrieval time and dispense error
    pelletDropTime = 0.0f;
    retrievalTime = 0.0f;
    dispenseError = false;
}

/********************************************************
 * Core Functions
 ********************************************************/
void FED4::update()
{
    updateTime();
    refreshSensors();
    updateDisplay();
    serialStatusReport();
    syncHublink();
}

/**
 * Legacy loop helper: update() then sleep(sleepSeconds).
 * Prefer waitUntil() + update() after feed for event-driven programs.
 */
void FED4::run()
{
    update();
    sleep();
}
