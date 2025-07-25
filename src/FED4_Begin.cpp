#include "FED4.h"

/********************************************************
 * Initialization
 * Initializes all components and sets up the FED4 system
 *
 * @return bool - true if initialization is successful, false otherwise
 */
//********************************************************

bool FED4::begin(const char *programName)
{
    Serial.begin(115200);

    // Initialize LDO2 to provide power to I2C peripherals
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);
    delay(100); // Stabilization time
    Serial.println();

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
        {"Light Sensor", {false, ""}},
        {"Touch Sensors", {false, ""}},
        {"Buttons", {false, ""}},
        {"Motor", {false, ""}},
        {"SD Card", {false, ""}},
        {"Display", {false, ""}},
        {"Speaker", {false, ""}},
        {"Accelerometer", {false, ""}},
        {"Magnet", {false, ""}},
        {"Motion", {false, ""}},
        {"ToF Sensor", {false, ""}}
#ifndef FED4_EXCLUDE_HUBLINK
        ,
        {"Hublink", {false, ""}}
#endif
    };

    // Initialize I2C buses
    statuses["I2C Primary"].initialized = Wire.begin(SDA, SCL);
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

    // Initialize battery monitor immediately after MCP (library requirement)
    Serial.println("Note: it is safe to ignore the three I2C warnings below");
    // Retry logic like the working test script
    int maxRetries = 3;
    int retryCount = 0;
    Serial.println("Initializing temperature/humidity sensor");
    while (!maxlipo.begin() && retryCount < maxRetries)
    {
        retryCount++;
        Serial.printf("Battery monitor init attempt %d failed, retrying...\n", retryCount);
        delay(100); // Wait before retry
    }

    statuses["Battery Monitor"].initialized = (retryCount < maxRetries);
    if (!statuses["Battery Monitor"].initialized)
    {
        Serial.println("Battery monitor initialization failed");
    }

    Serial.println("Initializing LDOs");
    // Initialize LDOs first
    statuses["LDOs"].initialized = initializeLDOs();

    Serial.println("Initializing NeoPixel");
    // Initialize LEDs
    statuses["NeoPixel"].initialized = initializePixel();
    redPix(1); // very dim red pix to indicate when FED4 is awake

    // Initialize RTC
    Serial.println("Initializing RTC");
    statuses["RTC"].initialized = initializeRTC();

    // Initialize temperature/humidity sensor directly
    Serial.println("Initializing temperature/humidity sensor");
    statuses["Temp/Humidity"].initialized = aht.begin(&I2C_2);
    if (!statuses["Temp/Humidity"].initialized)
    {
        Serial.println("Temperature/humidity sensor initialization failed");
    }

    // Initialize light sensor
    Serial.println("Initializing light sensor");
    statuses["Light Sensor"].initialized = initializeLightSensor();
    if (!statuses["Light Sensor"].initialized)
    {
        Serial.println("Light sensor initialization failed");
    }

    // startup front LEDs
    Serial.println("Initializing LED Strip");
    statuses["LED Strip"].initialized = initializeStrip();
    stripRainbow(3, 1);

    // Configure all GPIO pins
    Serial.println("Initializing GPIO pins");
    mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);
    mcp.pinMode(1, OUTPUT);    // Configure ToF sensor XSHUT pin (pin 1, not EXP_XSHUT_1)
    mcp.digitalWrite(1, HIGH); // Enable ToF sensor
    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT);
    pinMode(AUDIO_TRRS_3, INPUT);
    pinMode(USER_PIN_18, OUTPUT);
    digitalWrite(USER_PIN_18, LOW);

    // Configure haptic motor pin
    mcp.pinMode(EXP_HAPTIC, OUTPUT);
    mcp.digitalWrite(EXP_HAPTIC, LOW);

    // Initialize Touch
    Serial.println("Initializing Touch Sensors");
    statuses["Touch Sensors"].initialized = initializeTouch();
    calibrateTouchSensors();

    // Initialize Buttons
    Serial.println("Initializing Buttons");
    statuses["Buttons"].initialized = initializeButtons();
    if (!statuses["Buttons"].initialized)
    {
        Serial.println("Button initialization failed");
    }

    Serial.println("Initializing Motor");
    statuses["Motor"].initialized = initializeMotor();

    // Initialize SPI systems
    Serial.println("Initializing SPI");
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000); // Set SPI clock to 1MHz

    statuses["Accelerometer"].initialized = initializeAccel();

    statuses["Magnet"].initialized = initializeMagnet();
    if (!statuses["Magnet"].initialized)
    {
        Serial.println("Magnet sensor initialization failed");
    }

    stripRainbow(3, 1);

    // Initialize ToF sensor
    statuses["ToF Sensor"].initialized = initializeToF();
    if (!statuses["ToF Sensor"].initialized)
    {
        Serial.println("ToF sensor initialization failed");
    }

    // Initialize Motion sensor
    statuses["Motion"].initialized = initializeMotion();
    if (!statuses["Motion"].initialized)
    {
        Serial.println("Motion sensor initialization failed");
    }

    statuses["Display"].initialized = initializeDisplay();

    // check battery and environmental sensors
    startupPollSensors();

    // Low battery check and warning
    float voltage = getBatteryVoltage();
    if (voltage > 0 && voltage < 3.55)
    {
        displayLowBatteryWarning();
        delay(100);
        startSleep(); // Enter light sleep
    }

    // Initialize Speaker
    Serial.println("Initializing Speaker");
    statuses["Speaker"].initialized = initializeSpeaker();
    playTone(1000, 8, 0.3); // first playTone doesn't play for some reason - need to call once to get it going?

    // Initialize SD
    Serial.println("Initializing SD Card");
    statuses["SD Card"].initialized = initializeSD();

// Initialize Hublink if enabled
#ifndef FED4_EXCLUDE_HUBLINK
    if (useHublink)
    {
        Serial.println("Initializing Hublink");
        statuses["Hublink"].initialized = initializeHublink();
        if (!statuses["Hublink"].initialized)
        {
            Serial.println("Hublink initialization failed");
        }
    }
#endif

    // This is used to set the program name in the meta.json file
    // and to get the program name from the meta.json file
    //
    // Usage: When begin() is called in the FED4 Arduino script it can be called with a program name:
    //        begin("programName");
    //        programName will be used to set the program name in the meta.json file
    //        and change what shows up on the display and in the log file
    if (programName != nullptr)
    {
        setProgram(programName);
    }

    // Check for SD card errors and handle them
    if (!statuses["SD Card"].initialized)
    {
        Serial.println("SD Card initialization failed - handling error");
        sdCardAvailable = false;
        handleSDCardError();
    }
    else
    {
        // Try to create log file and check for filename creation errors
        Serial.println();
        Serial.println("Creating log file");
        bool logFileCreated = createLogFile();

        if (!logFileCreated)
        {
            Serial.println("Log file creation failed - handling error");
            sdCardAvailable = false;
            handleSDCardError();
        }
    }

    // Only pull JSON data from SD card if it's available
    if (sdCardAvailable)
    {
        Serial.println();
        Serial.println("Pulling JSON data from SD card:");
        program = getMetaValue("subject", "program");
        mouseId = getMetaValue("subject", "id");
        sex = getMetaValue("subject", "sex");
        strain = getMetaValue("subject", "strain");
        age = getMetaValue("subject", "age");

        // Check meta value
        String subjectId = getMetaValue("subject", "id");
        if (subjectId.length() > 0)
        {
            Serial.print(" - Subject ID: ");
            Serial.println(subjectId);
        }
    }
    else
    {
        // Set default values when SD card is not available
        Serial.println("SD card not available - using default values");
        if (program.length() == 0)
            program = "Default";
        if (mouseId.length() == 0)
            mouseId = "0000";
        if (sex.length() == 0)
            sex = "Unknown";
        if (strain.length() == 0)
            strain = "Unknown";
        if (age.length() == 0)
            age = "Unknown";
    }
    logData("Startup");

    stripRainbow(3, 1);

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

    startupAnimation();
    // three clicks to confirm initialization
    playTone(1000, 8, 0.5);
    delay(100);
    playTone(1000, 8, 0.5);
    delay(100);
    playTone(1000, 8, 0.5);

    return true;
}