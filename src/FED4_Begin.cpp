#include "FED4.h"

/********************************************************
 * Initialization
 * Initializes all components and sets up the FED4 system
 * 
 * @return bool - true if initialization is successful, false otherwise
 */ 
//********************************************************

bool FED4::begin(const char* programName)
{
    Serial.begin(115200);

    // Initialize LDO2 to provide power to I2C peripherals
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);
    delayMicroseconds(100); // Stabilization time
    Serial.println("LDO2 Enabled");

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
        {"Buttons", {false, ""}},
        {"Motor", {false, ""}},
        {"SD Card", {false, ""}},
        {"Display", {false, ""}},
        {"Speaker", {false, ""}},
        {"Accelerometer", {false, ""}},
        {"Magnet", {false, ""}},
        {"Motion", {false, ""}}}; 

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

    // Initialize LDOs first
    statuses["LDOs"].initialized = initializeLDOs();

    // Initialize LEDs 
    statuses["NeoPixel"].initialized = initializePixel();
    bluePix();
    statuses["LED Strip"].initialized = initializeStrip();

    // startup LED display
    //a 25ms delay and 1 loop takes 1.024 seconds to complete - 
    //this is blocking BTW but OK to take an extra second because it looks cool
    stripRainbow(25, 1);  
    
    // Configure all GPIO pins
    mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT);
    pinMode(AUDIO_TRRS_3, INPUT);
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
    
    // Initialize Buttons
    statuses["Buttons"].initialized = initializeButtons();
    if (!statuses["Buttons"].initialized)
    {
        Serial.println("Button initialization failed");
    }
    
    statuses["Motor"].initialized = initializeMotor();

    // Initialize SPI systems
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000); // Set SPI clock to 1MHz

    // Initialize SD and Display
    statuses["SD Card"].initialized = initializeSD();

    // This is used to set the program name in the meta.json file
    // and to get the program name from the meta.json file  
    //
    // Usage: When begin() is called in the FED4 Arduino script it can be called with a program name:
    //        begin("programName");
    //        programName will be used to set the program name in the meta.json file
    //        and change what shows up on the display and in the log file
    if (programName != nullptr) {
        setProgram(programName);
    }

    //pull JSON data from SD card to store in log file
    program = getMetaValue("subject", "program");     
    mouseId = getMetaValue("subject", "id");    
    sex = getMetaValue("subject", "sex");    
    strain = getMetaValue("subject", "strain"); 
    age = getMetaValue("subject", "age");

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

    statuses["Display"].initialized = initializeDisplay();
    startupAnimation();

    // check battery and environmental sensors
    startupPollSensors(); 

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