#include "FED4.h"
const char FED4::libraryVer[] = "1.0.0";

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

/**
 * Initializes all components and sets up the FED4 system
 *
 * @return bool - true if initialization is successful, false otherwise
 */
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
    startupAnimation();

    // check battery and environmental sensors
    startupPollSensors();

    // get JSON data from SD card
    program = getMetaValue("subject", "program"); // returns
    mouseId = getMetaValue("subject", "id");      // returns

    // initialize logging
    createLogFile();
    logData("Startup");

    // Initialize Speaker last (as in original)
    statuses["Speaker"].initialized = initializeSpeaker();
    // playStartup();  move to main sketch to easily turn off while working on a plane :)

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

    return true;
}

/********************************************************
 * Core Functions
 ********************************************************/
/**
 * Main run loop that updates time, display, prints status and handles sleep
 */
void FED4::run()
{
    updateTime();
    updateDisplay();
    serialStatusReport();
    sleep();
}

/**
 * Updates time variables from RTC
 */

void FED4::updateTime()
{
    DateTime current = rtc.now();
    currentHour = current.hour();     // useful for timed feeding sessions
    currentMinute = current.minute(); // useful for timed feeding sessions
    currentSecond = current.second(); // useful for timed feeding sessions
    unixtime = current.unixtime();
}

/**
 * Polls sensors at startup to get initial readings
 */
void FED4::startupPollSensors()
{
    unsigned long startTime = millis();
    float temp = -1;
    float hum = -1;

    // get temp with timeout
    while (millis() - startTime < 1000)
    { // 1 second timeout
        temp = getTemperature();
        if (temp > 5)
            break; // Valid reading obtained
        delay(10);
    }
    if (temp > 5)
        temperature = temp;

    // get humidity with timeout
    startTime = millis(); // Reset timer for humidity
    while (millis() - startTime < 1000)
    { // 1 second timeout
        hum = getHumidity();
        if (hum > 5)
            break; // Valid reading obtained
        delay(10);
    }
    if (hum > 5)
        humidity = hum;

    // get battery info with timeout
    startTime = millis();
    while (millis() - startTime < 1000)
    { // 1 second timeout
        cellVoltage = getBatteryVoltage();
        cellPercent = getBatteryPercentage();
        if (cellVoltage > 0)
            break; // Valid reading obtained
        delay(10);
    }

    if (cellPercent > 100)
    {
        cellPercent = 100;
    }
}

/**
 * Polls temperature, humidity and battery sensors periodically to update their values.
 * Only updates every 10 minutes to avoid excessive polling.
 * Uses timeouts to prevent hanging if sensors are unresponsive.
 */
void FED4::pollSensors()
{
    int minToUpdateSensors = 10; // update sensors every N minutes
    if (millis() - lastPollTime > (minToUpdateSensors * 60000))
    {
        lastPollTime = millis();
        // get temp and humidity with timeouts
        unsigned long startTime = millis();
        float temp = -1;
        float hum = -1;

        // get temp with timeout
        while (millis() - startTime < 1000)
        { // 1 second timeout
            temp = getTemperature();
            if (temp > 5)
                break; // Valid reading obtained
            delay(10);
        }
        if (temp > 5)
            temperature = temp;

        // get humidity with timeout
        startTime = millis(); // Reset timer for humidity
        while (millis() - startTime < 1000)
        { // 1 second timeout
            hum = getHumidity();
            if (hum > 5)
                break; // Valid reading obtained
            delay(10);
        }

        if (hum > 5)
            humidity = hum;

        // get battery info with timeout
        startTime = millis();
        while (millis() - startTime < 1000)
        { // 1 second timeout
            cellVoltage = getBatteryVoltage();
            cellPercent = getBatteryPercentage();
            if (cellVoltage > 0)
                break; // Valid reading obtained
            delay(10);
        }

        if (cellPercent > 100)
        {
            cellPercent = 100;
        }
    }
}

/**
 * Feeds the mouse by dispensing a pellet from the hopper
 */
void FED4::feed()
{
    bool pelletPresent = checkForPellet();
    bool pelletDropped = didPelletDrop();
    pelletReady = false;
    dispenseError = false;
    clearStrip();

    Serial.println("Feeding!");
    while (!pelletPresent && !pelletDropped) // while no pellet is present and none has dropped
    {
        redPix();
        // check if pellet has dropped or is present
        pelletDropped = didPelletDrop();
        pelletPresent = checkForPellet();
        pelletReady = true;

        // small motor movement
        stepper.step(-2);
        delay(10);
        motorTurns++;

        // delay for 1s roughly each pellet position
        if (motorTurns % 125 == 0)
        {
            releaseMotor();
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

        if (motorTurns > 10000) // how many motorTurns before FED4 stops trying and shuts off?  Each full rotation of the hopper is ~1000 motorTurns
        {
            jammed();
        }
    }

    orangePix();
    // think about this: if drop is not detected, we don't want to log an error, but we also don't want to log a pellet drop

    if (pelletReady)
    {
        Serial.println("PelletDrop");
        pelletDropTime = millis();
        pelletCount++;
        logData("PelletDrop");
        outputPulse(1, 100);
        outputPulse(2, 100);
    }
    motorTurns = 0;
    releaseMotor();

    // Wait up to 500ms for pellet to settle in well if 500ms passes without detection, set dispenseError to true
    unsigned long startWait = millis();
    bool pelletDetected = false;
    while (millis() - startWait < 500)
    {
        if (checkForPellet())
        {
            pelletDetected = true;
            break; // Exit if pellet is detected
        }
        delay(10);
    }
    if (!pelletDetected)
    {
        dispenseError = true;
    }

    // Calculate time since pellet drop
    pelletWellTime = millis();

    pelletPresent = checkForPellet();

    cyanPix();
    Serial.println("Pellet in Well");

    while (pelletPresent)
    { // while pellet is in well, monitor for pokes and retrieval time
        bluePix();
        pelletPresent = checkForPellet();

        retrievalTime = (millis() - pelletWellTime) / 1000.0;
        if (retrievalTime > 20)
            break;

        // log pokes while pellet is in well
        //  Read current touch values
        uint16_t leftVal = touchRead(TOUCH_PAD_LEFT);
        uint16_t centerVal = touchRead(TOUCH_PAD_CENTER);
        uint16_t rightVal = touchRead(TOUCH_PAD_RIGHT);

        // Check for significant deviations from baseline
        float leftDev = abs((float)leftVal / touchPadLeftBaseline - 1.0);
        float centerDev = abs((float)centerVal / touchPadCenterBaseline - 1.0);
        float rightDev = abs((float)rightVal / touchPadRightBaseline - 1.0);

        // Only log pokes if there's a significant deviation
        if (leftDev >= TOUCH_THRESHOLD)
        {
            leftCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            Serial.println("LeftWithPellet");
            logData("LeftWithPellet");
            click();
            updateDisplay();
            greenPix();
            outputPulse(1, 100);
            // wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_LEFT) / touchPadLeftBaseline - 1.0) >= TOUCH_THRESHOLD)
            {
                delay(10);
            }
        }
        else if (centerDev >= TOUCH_THRESHOLD)
        {
            centerCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            Serial.println("CenterWithPellet");
            logData("CenterWithPellet");
            click();
            updateDisplay();
            bluePix();
            outputPulse(2, 100);
            // wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_CENTER) / touchPadCenterBaseline - 1.0) >= TOUCH_THRESHOLD)
            {
                delay(10);
            }
        }
        else if (rightDev >= TOUCH_THRESHOLD)
        {
            rightCount++;
            retrievalTime = 0.0;
            dispenseError = false;
            Serial.println("RightWithPellet");
            logData("RightWithPellet");
            click();
            updateDisplay();
            redPix();
            outputPulse(3, 100);
            // wait for touch to return to baseline
            while (abs((float)touchRead(TOUCH_PAD_RIGHT) / touchPadRightBaseline - 1.0) >= TOUCH_THRESHOLD)
            {
                delay(10);
            }
        }
    }

    purplePix();
    Serial.println("Pellet Removed");

    if (pelletReady)
    {
        if (dispenseError)
        {
            logData("PelletNotDetected");
        }
        else
        {
            logData("PelletTaken");
            outputPulse(1, 100);
            outputPulse(3, 100);
        }
    }

    // Reset variables
    pelletReady = false;
    retrievalTime = 0;
    dispenseError = false;

    // Reset touch states after handling the feed
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;

    // Rebaseline touch sensors
    reBaselineTouches = 10;
    if ((leftCount + rightCount + centerCount) % reBaselineTouches == 0 && (leftCount + rightCount + centerCount) > 5)
    {
        calibrateTouchSensors();
    }

    pollSensors(); // only poll sensors after a feed event so we don't block responsiveness after pokes
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
    return !mcp.digitalRead(EXP_PHOTOGATE_4);
}