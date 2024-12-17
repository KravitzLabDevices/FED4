#include "FED4.h"

// (o)(o)--.
//  \../ (  )
//  m\/m--m'`--.

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : Adafruit_GFX(DISPLAY_WIDTH, DISPLAY_HEIGHT),
               pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
               stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4),
               I2C_2(1)
{
    pelletReady = true;
    feedReady = false;
    displayBuffer = nullptr; // Initialize our display buffer pointer
    vcom = false;            // Initialize VCOM state

    // Initialize counters
    pelletCount = 0;
    centerCount = 0;
    leftCount = 0;
    rightCount = 0;
    wakeCount = 0;
    motorTurns = 0;

    // Initialize touch states
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
}

/********************************************************
 * Initialization
 ********************************************************/
// !! this could benefit from every init returning a bool, then
// !! showing a status report or acting on critical errors
void FED4::begin()
{
    Serial.begin(115200);

    // Initialize CS pins to their inactive states
    pinMode(SD_CS, OUTPUT);
    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);     // SD inactive = HIGH
    digitalWrite(DISPLAY_CS, LOW); // Display inactive = LOW

    initializeLDOs(); // turns on LDO2 and LDO3 by default
    LDO3_OFF();       // only attached to User Pins connnector

    // Initialize primary I2C bus
    if (!Wire.begin())
    {
        Serial.println("I2C Error - check I2C Address");
    }

    // Initialize I2C on the second bus
    if (!I2C_2.begin(SDA_2, SCL_2))
    {
        Serial.println("I2C_2 Error - check I2C Address");
        while (1)
            ;
    }

    // Initialize GPIO expander
    if (!mcp.begin_I2C())
    {
        Serial.println("mcp error");
    }
    else
    {
        Serial.println("mcp ok");
    }

    initializeLEDs();
    initializeRTC();
    initializeVitals();

    // Configure GPIO pins
    mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);

    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT);
    pinMode(AUDIO_TRRS_3, INPUT);

    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT);
    pinMode(BUTTON_3, INPUT);
    pinMode(USER_PIN_18, OUTPUT);
    digitalWrite(USER_PIN_18, LOW); // using as wakeup pulse in FED_Power.cpp

    initializeTouch();
    calibrateTouchSensors();
    initializeMotor();

    // Initialize SPI once for all devices
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    // Initialize SPI for SD card
    initializeSD();      // Initialize SD after display is ready
    initializeDisplay(); // This will initialize our native display
    updateDisplay();     // Update the display with initial content

    // example usage of getMetaValue
    // String subjectId = getMetaValue("subject", "id");
    // if (subjectId.length() > 0)
    // {
    //     Serial.print("Subject ID: ");
    //     Serial.println(subjectId);
    // }

    createLogFile();
    setEvent("Startup");
    logData();

    // not working for some reason, just makes static
    // initializeSpeaker();
    // playStartup();
}

/********************************************************
 * Core Functions
 ********************************************************/
void FED4::feed()
{
    if (feedReady)
    {
        bool pelletPresent = checkForPellet();

        Serial.println("Feeding!");
        while (pelletPresent)
        {                     // while pellet well is empty
            stepper.step(-2); // small movement
            delay(10);
            pelletPresent = checkForPellet();
            pelletReady = true;
            motorTurns++;

            // delay for 1s roughly each pellet position
            if (motorTurns % 125 == 0)
            {
                delay(1000);
            }

            // if stepper is called too many times without a dispense do a small movement to remove jam
            if (motorTurns % 500 == 0)
            {
                minorJamClear();
            }

            if (motorTurns % 1000 == 0)
            {
                vibrateJamClear();
            }
        }
        motorTurns = 0;

        if (pelletReady)
        {
            pelletCount++;
            pelletReady = false;
        }

        feedReady = false;
        releaseMotor();
        setEvent("PelletDrop");
        //        logData();

        // Monitor retrieval
        unsigned long pelletDrop = millis();
        while (!pelletPresent)
        { // while pellet well is full
            bluePix();
            pelletPresent = checkForPellet();
            retrievalTime = millis() - pelletDrop;
            if (retrievalTime > 10000)
                break;
        }
        redPix();
        setEvent("PelletTaken");
        //        logData();
        retrievalTime = 0;

        // Reset touch states after handling the feed
        leftTouch = false;
        centerTouch = false;
        rightTouch = false;
    }

    updateDisplay();

    // Rebaseline touch sensors
    reBaselineTouches = 10;
    if ((leftCount + rightCount + centerCount) % reBaselineTouches == 0 && (leftCount + rightCount + centerCount) > 5)
    {
        calibrateTouchSensors();
    }
}

bool FED4::checkForPellet()
{
    return mcp.digitalRead(EXP_PHOTOGATE_1);
}
