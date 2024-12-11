#include "FED4.h"

// (o)(o)--.
//  \../ (  )
//  m\/m--m'`--.

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : display(SPI_SCK, SPI_MOSI, DISPLAY_CS, DISPLAY_WIDTH, DISPLAY_HEIGHT),
               pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
               stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4),
               I2C_2(1)
{
    pelletReady = true;
    feedReady = false;

    // Initialize counters
    pelletCount = 0;
    centerCount = 0;
    leftCount = 0;
    rightCount = 0;
    wakeCount = 0;
}

/********************************************************
 * Initialization
 ********************************************************/
void FED4::begin()
{
    Serial.begin(115200);
    initializeLDOs(); // turns on LDO2 and LDO3 by default

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
    Serial.println("mcp ok");

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
    monitorTouchSensors(); // !! loops if BUTTON_2 (center) is pressed

    initializeSD();
    createLogFile();
    setEvent("Startup");
    logData();

    initializeDisplay();
}

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
        }

        if (pelletReady)
        {
            pelletCount++;
            pelletReady = false;
        }
        feedReady = false;

        releaseMotor();
        setEvent("PelletDrop");

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
        retrievalTime = 0;
    }

    updateDisplay();

    // Rebaseline touch sensors every 5 pellets
    if (pelletCount % 5 == 0 && pelletCount > 1)
    {
        calibrateTouchSensors();
    }
}

bool FED4::checkForPellet()
{
    return mcp.digitalRead(EXP_PHOTOGATE_1);
}