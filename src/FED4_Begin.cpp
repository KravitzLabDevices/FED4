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
    
    // Initialize state flags
    motionSensorInitialized = false;

    // Initialize LDO2 to provide power to I2C peripherals
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);
    delay(10); // Stabilization time
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
        {"ToF Sensor", {false, ""}},
        {"Drop Sensor", {false, ""}}
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
    I2C_2.setClock(400000);  // Set I2C_2 to 400kHz (fast mode) for motion sensor
    
    // Allow I2C buses to stabilize before accessing devices
    delay(10);

    // Initialize MCP expander
    statuses["MCP23017"].initialized = mcp.begin_I2C();
    if (!statuses["MCP23017"].initialized)
    {
        Serial.println("MCP error");
    }
    
    // Allow MCP to stabilize before accessing other I2C devices
    delay(10);

    // Initialize battery monitor immediately after MCP (library requirement)
    // Retry logic like the working test script
    int maxRetries = 3;
    int retryCount = 0;
    Serial.println("Initializing battery monitor");
    Serial.println("Note: it is safe to ignore the three I2C warnings below");
    while (!maxlipo.begin() && retryCount < maxRetries)
    {
        retryCount++;
        Serial.printf("Battery monitor init attempt %d failed, retrying...\n", retryCount);
        delay(10); // Wait before retry
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

    // Initialize temperature/humidity/pressure/gas sensor BME680
    // Temporarily reduce primary I2C speed for sensor initialization (some sensors are sensitive to high speeds)
    Wire.setClock(100000);  // Set to 100kHz for sensor initialization
    delay(5);  // Brief delay to allow clock change to take effect
    Serial.println("Initializing BME680 temperature/humidity/pressure/gas sensor");
    statuses["Temp/Humidity"].initialized = bme.begin(0x76, &Wire);
    if (!statuses["Temp/Humidity"].initialized)
    {
        Serial.println("BME680 sensor initialization failed - check wiring on pins 8 & 9!");
    }
    
    // Restore primary I2C to 400kHz
    Wire.setClock(400000);
    delay(5);  // Brief delay to allow clock change to take effect

    // Initialize light sensor
    Serial.println("Initializing light sensor");
    statuses["Light Sensor"].initialized = initializeLightSensor();
    if (!statuses["Light Sensor"].initialized)
    {
        Serial.println("Light sensor initialization failed");
    }
    
    // Restore I2C_2 to 400kHz for motion sensor (if it will be initialized later)
    I2C_2.setClock(400000);
    delay(5);  // Brief delay to allow clock change to take effect

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

    // Initialize Motion sensor (if enabled)
    if (useMotionSensor) {
        statuses["Motion"].initialized = initializeMotion();
        if (!statuses["Motion"].initialized)
        {
            Serial.println("Motion sensor initialization failed");
        }
    } else {
        statuses["Motion"].initialized = true; // Mark as "initialized" (skipped)
        Serial.println("Motion sensor (STHS34PF80) disabled by flag");
    }

    // Initialize Drop sensor
    statuses["Drop Sensor"].initialized = initializeDropSensor();
    if (!statuses["Drop Sensor"].initialized)
    {
        Serial.println("Drop sensor not detected or not working");
    }

    statuses["Display"].initialized = initializeDisplay();

    // Prepare I2C buses for sensor polling
    // Reduce I2C speeds for reliable sensor reads (some sensors are sensitive to high speeds)
    Wire.setClock(100000);  // Set primary I2C to 100kHz for BME680 and battery monitor reads
    I2C_2.setClock(100000);  // Set secondary I2C to 100kHz for light sensor reads
    delay(10);  // Allow clock changes to take effect
    
    // Clear I2C buses to reset any stuck states
    Wire.beginTransmission(0x00);
    Wire.endTransmission();
    I2C_2.beginTransmission(0x00);
    I2C_2.endTransmission();
    delay(5);

    // check battery and environmental sensors
    startupPollSensors();
    
    // Restore I2C_2 to 400kHz for motion sensor (if it will be used later)
    I2C_2.setClock(400000);
    delay(5);

    // Low battery check and warning
    float voltage = getBatteryVoltage();
    if (voltage > 0 && voltage < 3.55)
    {
        displayLowBatteryWarning();
        delay(50);
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
        program = getMetaValue("fed", "program");  // Changed from "subject" to "fed" to match menu
        mouseId = getMetaValue("subject", "id");
        sex = getMetaValue("subject", "sex");
        strain = getMetaValue("subject", "strain");
        age = getMetaValue("subject", "age");

        // Debug output for program loading
        Serial.print(" - Program loaded: '");
        Serial.print(program);
        Serial.println("'");

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
    lightsOff();
    
    // temporarily unmute audio even if it is silenced
    digitalWrite(AUDIO_SD, HIGH);
    click();
    delay (100);

    click();
    delay (100);

    click();
    delay (100);

    clearDisplay();

    // Reset pollSensorsTimer so seconds display resets when data is written
    pollSensorsTimer = millis();
    
    // Check if mouseId is 99 - if so, launch Pong game
    if (mouseId == "99" || mouseId == "0099") {
        Serial.println("MouseID 99 detected - launching Pong game!");
        greenPix(5);
        delay(200);
        
        // Launch Pong game 
        while (true) {
            pong();
        }
    }
    return true;
}