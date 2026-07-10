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
    
    Serial.println();

    // Structure to track component status
    struct ComponentStatus
    {
        bool initialized;
        const char *notes; // Example: statuses["Battery"].notes = "3.7V"; or "Init failed - timeout"
    };

    // Use map to store status by component name
    std::map<const char *, ComponentStatus, std::less<>> statuses = {
        {"Power Rails", {false, ""}},
        {"Status LED", {false, ""}},
        {"LED Strip", {false, ""}},
        {"I2C Primary", {false, ""}},
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
        {"ToF Sensor", {false, ""}},
        {"Drop Sensor", {false, ""}},
        {"Solenoids", {false, ""}},
        {"INT_OR", {false, ""}}
#ifndef FED4_EXCLUDE_HUBLINK
        ,
        {"Hublink", {false, ""}}
#endif
    };

    // Initialize SPI systems
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000); // Set SPI clock to 1MHz

    // Initialize primary I2C bus
    displayInitStatus("I2C Primary");
    statuses["I2C Primary"].initialized = Wire.begin(SDA, SCL);
    if (!statuses["I2C Primary"].initialized)
    {
        Serial.println("I2C Error - check I2C Address");
        return false;
    }
    // I2C bus runs at 100kHz (ESP32 default) for reliable operation across all sensors
    delay(2);

    // Initialize MCP GPIO expander (on always-on 3.3V rail)
    displayInitStatus("GPIO expander");
    statuses["MCP23017"].initialized = mcp.begin_I2C();
    if (!statuses["MCP23017"].initialized)
    {
        Serial.println("MCP error");
    }
    
    // Allow MCP to stabilize before accessing other I2C devices
    delay(1);

    // Enable PSV2 and PSV3 power rails via MCP expander
    Serial.println("Initializing Power Rails");
    displayInitStatus("Power management");
    statuses["Power Rails"].initialized = initializePower();

    // Reset display and turn on backlight before initializing
    displayReset();
    displayLight(true);

    // Initialize display now that power rails and MCP are ready
    statuses["Display"].initialized = initializeDisplay();
    Serial.println("Starting up...");
    startupAnimation();

    // Initialize battery monitor (MAX17048 on VBATT rail)
    displayInitStatus("Battery Monitor");
    int maxRetries = 3;
    int retryCount = 0;
    Serial.println("Initializing Battery Monitor");
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

    // Initialize status LED (single red LED, always-on 3.3V rail)
    Serial.println("Initializing Status LED");
    displayInitStatus("Status LED");
    statuses["Status LED"].initialized = initializePixel();
    redPix(1); // very dim red to indicate FED4 is awake

    // Initialize RTC (behind TCA4307 on PSV2 rail - must be after PSV2_ON)
    Serial.println("Initializing RTC");
    displayInitStatus("RTC");
    statuses["RTC"].initialized = initializeRTC();

    // Initialize temperature/humidity/pressure/gas sensor BME680
    displayInitStatus("Temp/Humidity");
    Serial.println("Initializing BME680 temperature/humidity/pressure/gas sensor");
    statuses["Temp/Humidity"].initialized = bme.begin(I2C_ADDR_BME680, &Wire);
    if (!statuses["Temp/Humidity"].initialized)
    {
        Serial.println("BME680 sensor initialization failed - check wiring on pins 8 & 9!");
    }

    // Initialize light sensor
    Serial.println("Initializing Light Sensor");
    displayInitStatus("Light Sensor");
    statuses["Light Sensor"].initialized = initializeLightSensor();
    if (!statuses["Light Sensor"].initialized)
    {
        Serial.println("Light sensor initialization failed");
    }

    // Initialize front LED strip (PSV3 rail)
    Serial.println("Initializing LED Strip");
    statuses["LED Strip"].initialized = initializeStrip();
    stripRainbow(3, 1);

    // Configure GPIO pins
    Serial.println("Initializing GPIO pins");
    // Photogates are on PSV2 rail - configure direct GPIO
    pinMode(PHOTOGATE_1, INPUT_PULLUP);
    pinMode(PHOTOGATE_4, INPUT_PULLUP);
    // TRRS pins (always-on rail) - pulled up so they read HIGH when nothing is connected
    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_3, INPUT_PULLUP);
    // PIR motion sensor (always-on rail) - digital push-pull output, no protocol needed
    if (useMotionSensor) {
        pinMode(PIR_MOTION, INPUT_PULLDOWN);
    }

    // Configure haptic motor pin on expander (PSV2 rail)
    mcp.pinMode(EXP_HAPTIC, OUTPUT);
    mcp.digitalWrite(EXP_HAPTIC, LOW);

    // Initialize Touch
    Serial.println("Initializing Touch Sensors");
    displayInitStatus("Touch Sensors");
    statuses["Touch Sensors"].initialized = initializeTouch();
    calibrateTouchSensors(true);  // Check stability at startup

    // Initialize Buttons
    Serial.println("Initializing Buttons");
    statuses["Buttons"].initialized = initializeButtons();
    if (!statuses["Buttons"].initialized)
    {
        Serial.println("Button initialization failed");
    }

    Serial.println("Initializing Motor");
    displayInitStatus("Motor drive");
    statuses["Motor"].initialized = initializeMotor();

    displayInitStatus("Accelerometer");
    statuses["Accelerometer"].initialized = initializeAccel();

    stripRainbow(3, 1);

    // Initialize ToF sensor (always-on 3.3V, no XSHUT)
    displayInitStatus("Proximity Sensor");
    statuses["ToF Sensor"].initialized = initializeToF();
    if (!statuses["ToF Sensor"].initialized)
    {
        Serial.println("ToF sensor initialization failed");
    }

    // Initialize Drop sensor (photogate on PSV2 rail)
    displayInitStatus("Drop Sensor");
    statuses["Drop Sensor"].initialized = initializeDropSensor();
    if (!statuses["Drop Sensor"].initialized)
    {
        Serial.println("Drop sensor not detected or not working");
    }

    // Initialize solenoids
    displayInitStatus("Solenoids");
    statuses["Solenoids"].initialized = initializeSolenoids();

    // Initialize INT_OR interrupt subsystem (active-LOW, scans ToF/RTC/BAT/PIR/Accel)
    displayInitStatus("Interrupts");
    statuses["INT_OR"].initialized = initializeInterrupts();

    // Clear I2C bus to reset any stuck states before sensor polling
    Wire.beginTransmission(0x00);
    Wire.endTransmission();
    delay(5);

    // Check battery and environmental sensors
    startupPollSensors();

    // Low battery check and warning
    float voltage = getBatteryVoltage();
    if (voltage > 0 && voltage < 3.55)
    {
        displayLowBatteryWarning();
        delay(50);
        startSleep(); // Enter light sleep
    }

    // Initialize Speaker (amp SD on PSV2 rail via MCP)
    Serial.println("Initializing Speaker");
    displayInitStatus("Audio output");
    statuses["Speaker"].initialized = initializeSpeaker();
    playTone(1000, 8, 0.3); // first playTone doesn't play for some reason - need to call once to get it going

    // Prepare SPI bus for SD card initialization
    // Ensure display CS is deselected (display uses LOW when inactive)
    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(DISPLAY_CS, LOW);
    
    // Small delay to allow SPI bus to stabilize after display operations
    delay(10);
    
    // Initialize SD
    Serial.println("Initializing SD Card");
    displayInitStatus("SD Card");
    statuses["SD Card"].initialized = initializeSD();

    // Cold-boot recovery window: some cards need extra time/attempts on power-up.
    // If init failed, keep trying briefly before entering the blocking error UI.
    if (!statuses["SD Card"].initialized)
    {
        const unsigned long recoveryWindowMs = 2500;
        const unsigned long retryDelayMs = 250;
        unsigned long startMs = millis();

        Serial.printf("SD init failed; retrying for up to %lu ms...\n", (unsigned long)recoveryWindowMs);
        while (!statuses["SD Card"].initialized && (millis() - startMs) < recoveryWindowMs)
        {
            delay(retryDelayMs);
            statuses["SD Card"].initialized = initializeSD();
        }

        if (statuses["SD Card"].initialized)
        {
            Serial.println("SD recovered during startup retry window");
        }
    }

    // Initialize Hublink if enabled
    #ifndef FED4_EXCLUDE_HUBLINK
        if (useHublink)
        {
            Serial.println("Initializing Hublink");
            displayInitStatus("Hublink");
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

    lightsOff();
    
    // Temporarily unmute audio for startup clicks
    mcp.digitalWrite(EXP_AMP_SD, HIGH);
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
        redPix(5);
        delay(200);
        
        // Launch Pong game 
        while (true) {
            pong();
        }
    }
    return true;
}
