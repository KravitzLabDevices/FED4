#include "FED4.h"

// (o)(o)--.
//  \../ (  )
//  m\/m--m'`--.

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : Adafruit_GFX(DISPLAY_WIDTH, DISPLAY_HEIGHT),
               pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
               strip(8, RGB_STRIP_PIN, NEO_GRB + NEO_KHZ800),
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
bool FED4::begin()
{
    Serial.begin(115200);

    // Structure to track component status
    struct ComponentStatus
    {
        bool initialized;
        const char *notes; // Example: statuses["Battery"].notes = "3.7V"; or "Init failed - timeout"
    };

    // Use map to store status by component name
    std::map<const char *, ComponentStatus, std::less<>> statuses = {
        {"LDOs", {false, ""}},
        {"NeoPixel", {false, ""}},
        {"LED Strip", {false, ""}},
        {"I2C Primary", {false, ""}},
        {"I2C Secondary", {false, ""}},
        {"MCP23017", {false, ""}},
        {"RTC", {false, ""}},
        {"Battery Monitor", {false, ""}},
        {"Temp/Humidity", {false, ""}},
        {"Touch Sensors", {false, ""}},
        {"Motor", {false, ""}},
        {"SD Card", {false, ""}},
        {"Display", {false, ""}},
        {"Speaker", {false, ""}},
        {"Accelerometer", {false, ""}},
        {"Magnet", {false, ""}},
        {"Motion", {false, ""}},
        {"ToF Sensors", {false, ""}}};

    // Initialize LDOs first
    statuses["LDOs"].initialized = initializeLDOs();
    LDO3_OFF();

    // Initialize LEDs early (as in original)
    statuses["NeoPixel"].initialized = initializePixel();
    bluePix();
    statuses["LED Strip"].initialized = initializeStrip();
    stripRainbow(1, 1);

    // Initialize I2C buses
    statuses["I2C Primary"].initialized = Wire.begin();
    if (!statuses["I2C Primary"].initialized)
    {
        Serial.println("I2C Error - check I2C Address");
        return false;
    }

    statuses["I2C Secondary"].initialized = I2C_2.begin(SDA_2, SCL_2);
    if (!statuses["I2C Secondary"].initialized)
    {
        Serial.println("I2C_2 Error - check I2C Address");
        return false;
    }

    // Initialize MCP expander
    statuses["MCP23017"].initialized = mcp.begin_I2C();
    if (!statuses["MCP23017"].initialized)
    {
        Serial.println("MCP error");
    }
    else
    {
        Serial.println("MCP ok");
    }

    // Configure all GPIO pins
    mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT);
    pinMode(AUDIO_TRRS_3, INPUT);
    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT);
    pinMode(BUTTON_3, INPUT);
    pinMode(USER_PIN_18, OUTPUT);
    digitalWrite(USER_PIN_18, LOW);

    // Initialize RTC and Vitals
    statuses["RTC"].initialized = initializeRTC();
    bool vitalsResult = initializeVitals();
    if (!vitalsResult)
    {
        Serial.println("Vitals initialization failed");
    }
    statuses["Battery Monitor"].initialized = maxlipo.begin();
    statuses["Temp/Humidity"].initialized = aht.begin(&I2C_2);

    // Initialize Touch and Motor
    statuses["Touch Sensors"].initialized = initializeTouch();
    calibrateTouchSensors();
    statuses["Motor"].initialized = initializeMotor();

    // Initialize SPI systems
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000); // Set SPI clock to 1MHz

    // Initialize SD and Display
    statuses["SD Card"].initialized = initializeSD();

    // Check meta value
    String subjectId = getMetaValue("subject", "id");
    if (subjectId.length() > 0)
    {
        Serial.print("Subject ID: ");
        Serial.println(subjectId);
    }

    statuses["Accelerometer"].initialized = initializeAccel();

    statuses["Magnet"].initialized = initializeMagnet();
    if (!statuses["Magnet"].initialized)
    {
        Serial.println("Magnet sensor initialization failed");
    }

    statuses["Motion"].initialized = initializeMotionSensor();
    if (!statuses["Motion"].initialized)
    {
        Serial.println("Motion sensor initialization failed");
    }

    statuses["ToF Sensors"].initialized = initializeToF();
    if (!statuses["ToF Sensors"].initialized)
    {
        Serial.println("ToF sensors initialization failed");
    }

    statuses["Display"].initialized = initializeDisplay();
    updateDisplay();

    // Initialize logging
    createLogFile();
    setEvent("Startup");
    logData();

    // Initialize Speaker last (as in original)
    statuses["Speaker"].initialized = initializeSpeaker();
    playStartup();

    // Print initialization report
    Serial.println("\n=== FED4 Initialization Report ===");
    Serial.println("Component          Status  Notes");
    Serial.println("--------------------------------");

    int failCount = 0;
    for (const auto &component : statuses)
    {
        if (!component.second.initialized)
            failCount++;
        Serial.printf("%-18s %s %s\n",
                      component.first,
                      component.second.initialized ? "OK   " : "FAIL ",
                      component.second.notes);
    }

    Serial.println("--------------------------------");
    Serial.printf("Summary: %d/%d components initialized\n",
                  statuses.size() - failCount,
                  statuses.size());
    Serial.println("================================\n");

    serialStatusReport();

    return true;
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
