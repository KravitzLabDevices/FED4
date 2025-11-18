#include "FED4.h"

// This chip does not like SPI.beginTransaction(SPISettings(SHARPMEM_SPI_FREQ, LSBFIRST, SPI_MODE0));
// SPI will stop working. The display can operate on SPI_MODE0, just LSBFIRST.
// So we need to set the bit order to MSBFIRST for SD operations.

/********************************************************
 * SD Card Functions
 ********************************************************/

/**
 * Initializes the SD card
 * @return true if successful, false otherwise
 */
bool FED4::initializeSD()
{
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH); // SD inactive = HIGH
    SPI.setBitOrder(MSBFIRST);

    // Initialize SD card
    if (SD.begin(SD_CS, SPI, 4000000))
    {
        createMetaJson(); // Ensure meta.json exists
        return true;
    }
    Serial.println("SD card initialization failed");
    return false;
}

/**
 *  s the meta.json file with default structure if it doesn't exist
 * Default structure:
 * {
 *     "subject": {
 *         "id": "",
 *         "sex": "",
 *         "strain": ""
 *     },
 *     "fed": {
 *         "program": ""
 *     }
 * }
 * @return true if successful or file already exists, false if creation failed
 */
bool FED4::createMetaJson()
{
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW); // Select SD card for operation

    // Check if file already exists
    if (SD.exists(META_FILE))
    {
        digitalWrite(SD_CS, HIGH);
        return true;
    }

    // Create the JSON document
    const size_t capacity = JSON_OBJECT_SIZE(2) + // For "subject" and "fed"
                            JSON_OBJECT_SIZE(2) + // For "id" and "sex" under "subject"
                            JSON_OBJECT_SIZE(1) + // For "program" under "fed"
                            60;                   // Extra space for strings
    DynamicJsonDocument doc(capacity);

    // Create the structure
    JsonObject subject = doc.createNestedObject("subject");
    subject["id"] = "";
    subject["sex"] = "";
    subject["strain"] = "";
    subject["age"] = "";

    JsonObject fed = doc.createNestedObject("fed");
    fed["program"] = "";

    // Open file for writing
    File metaFile = SD.open(META_FILE, FILE_WRITE);
    if (!metaFile)
    {
        digitalWrite(SD_CS, HIGH);
        Serial.println("Failed to create meta.json");
        return false;
    }

    // Write the JSON structure
    bool success = serializeJson(doc, metaFile) > 0;
    metaFile.close();

    digitalWrite(SD_CS, HIGH); // Deselect after operations

    if (success)
    {
        Serial.println("Created meta.json with default structure");
    }
    else
    {
        Serial.println("Failed to write to meta.json");
    }

    return success;
}

/**
 * Creates a new log file with headers
 * @return true if successful, false if failed
 */
bool FED4::createLogFile()
{
    DateTime now = rtc.now();
    char idStr[5];
    int mouseIdValue = mouseId.toInt();  // Convert String to int
    if (mouseIdValue <= 0) {
        mouseIdValue = 0;
    }
    sprintf(idStr, "%04d", mouseIdValue);
    String id = String(idStr);
    char baseFilename[50];
    int fileNumber = 0;

    // Just change bit order for SD operations before file operations
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW);

    do
    {
        snprintf(baseFilename, sizeof(baseFilename), "/FED4_%s_%04d%02d%02d_%02d.CSV",
                 id.c_str(), now.year(), now.month(), now.day(), fileNumber);
        
        // Check if file exists
        if (!SD.exists(baseFilename)) {
            break;
        }
        
        // File exists, count the lines
        File dataFile = SD.open(baseFilename, FILE_READ);
        if (!dataFile) {
            fileNumber++;
            continue;
        }
        
        int lineCount = 0;
        while (dataFile.available()) {
            if (dataFile.read() == '\n') {
                lineCount++;
            }
        }
        dataFile.close();
                
        if (lineCount <= 5) {
            // File has 5 or fewer lines, delete and reuse this filename
            if (SD.remove(baseFilename)) {
                Serial.print("Removed incomplete file: ");
                Serial.println(baseFilename);
                // Verify it's actually gone
                if (!SD.exists(baseFilename)) {
                    break;
                }
                Serial.println("Warning: File still exists after remove");
            } else {
                Serial.print("Failed to remove file: ");
                Serial.println(baseFilename);
            }
            // If removal failed, try next file number
            fileNumber++;
            continue;
        }
        
        fileNumber++;
    } while (fileNumber < 100);

    digitalWrite(SD_CS, HIGH); // Deselect after file checks

    // Copy final filename to class member - ensure null termination
    strncpy(filename, baseFilename, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    // Just change bit order for SD operations
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW);

    // If file somehow still exists, try to remove it one more time
    if (SD.exists(filename)) {
        Serial.print("File already exists, attempting to remove: ");
        Serial.println(filename);
        if (!SD.remove(filename)) {
            digitalWrite(SD_CS, HIGH);
            Serial.println("Failed to remove existing file");
            filename[0] = '\0';
            return false;
        }
        delay(10); // Give SD card time to complete deletion
    }

    // Create new file and write headers
    File dataFile = SD.open(filename, FILE_WRITE);
    if (!dataFile)
    {
        digitalWrite(SD_CS, HIGH);
        Serial.println("Failed to create log file");
        filename[0] = '\0'; // Set filename to empty string on failure
        return false;
    }

    // Write CSV headers
    dataFile.print("DateTime,ElapsedSeconds,ESP32_UID,MouseID,Sex,Strain,LibraryVer,Program,FR,");
    dataFile.print("Event,PelletCount,LeftCount,RightCount,CenterCount,BlockPelletCount,BlockPokeCount,RetrievalTime,DispenseError,MotorTurns,Motion,");
    dataFile.println("Temperature,Humidity,Lux,White,FreeHeap,HeapSize,MinFreeHeap,WakeCount,BatteryVoltage,BatteryPercent");
    
    dataFile.flush();  // Force write to SD card
    dataFile.close();
    
    // Give SD card time to complete write operations
    delay(100);

    Serial.print("New file created: ");
    Serial.println(filename);

    digitalWrite(SD_CS, HIGH); // Deselect after operations
    return true;
}

/**
 * Logs data to the SD card
 * @param newEvent the event to log
 * @return true if successful, false if failed
 */
bool FED4::logData(const String &newEvent)
{
    // Check if SD card available flag is set
    if (!sdCardAvailable) {
        return false;
    }
    
    // Set new event if provided
    if (newEvent.length() > 0)
    {
        setEvent(newEvent);
    }

    File dataFile;
    SPI.setBitOrder(MSBFIRST);
    cyanPix(1); //dim cyan every time logData is called

    DateTime now = rtc.now();
    float currentSeconds = round((millis() / 1000.000) * 1000) / 1000.0; // Get current seconds rounded to 3 decimals

    // Open file for writing
    digitalWrite(SD_CS, LOW); // Select SD card for operation

    //SD.open() with a timeout
    unsigned long timeout = millis() + 500;
    do {
        dataFile = SD.open(filename, FILE_APPEND);
        if (!dataFile) delay(10);
    } while (!dataFile && millis() < timeout);

    // If the file is not found, try to reinitialize the SD card - this allows for hot swapping of the SD card
    if (!dataFile)
    {
        Serial.println("Failed to open log file for writing");
        Serial.print("Attempting to reinitialize SD card...");
        
        // Properly restart SPI interface
        SPI.end();
        delay(10); // Allow SPI to fully shut down
        
        // Reset the CS pin to ensure clean state
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);
        delay(1);
        
        // Restart SPI with proper initialization
        SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
        SPI.setBitOrder(MSBFIRST);
        SPI.setDataMode(SPI_MODE0);
        delay(10); // Allow SD card to stabilize
                       
        // Test file system access
        if (!SD.exists("/")) {
            SD.end();
            delay(10);
            
            if (!SD.begin(SD_CS, SPI, 4000000)) { 
                Serial.println("Failed");
                digitalWrite(SD_CS, HIGH);
                return false;
            }
        }
        
        delay(10); // Allow SD card to stabilize
         
        // Check if our specific file exists
        if (!SD.exists(filename)) {
            Serial.println("File not found after reinit");
            digitalWrite(SD_CS, HIGH);
            return false;
        }

        //SD.open() with a timeout
        unsigned long timeout = millis() + 500;
        do {
            dataFile = SD.open(filename, FILE_APPEND);
            if (!dataFile) delay(10);
        } while (!dataFile && millis() < timeout);

        if (!dataFile) {
            Serial.println("Failed to open file even though it exists");
            Serial.print("SD card type: ");
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE) {
                Serial.println("No SD card");
            } else if (cardType == CARD_MMC) {
                Serial.println("MMC");
            } else if (cardType == CARD_SD) {
                Serial.println("SDSC");
            } else if (cardType == CARD_SDHC) {
                Serial.println("SDHC");
            } else {
                Serial.println("UNKNOWN");
            }
            digitalWrite(SD_CS, HIGH);
            noPix();
            return false;
        }
        Serial.println("Success!");
        sdCardAvailable = true;
    }

    // Write timestamp
    dataFile.printf("%04d-%02d-%02d %02d:%02d:%02d,%f,%llX,",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second(),
                    currentSeconds,
                    ESP.getEfuseMac());

    // Write mouse ID and other info
    char formattedMouseId[8];
    int mouseIdValue = mouseId.toInt();
    if (mouseIdValue <= 0 || mouseIdValue > 9999) {
        // Handle invalid or out-of-range values
        snprintf(formattedMouseId, sizeof(formattedMouseId), "%.4s", mouseId.c_str());
    } else {
        snprintf(formattedMouseId, sizeof(formattedMouseId), "%04d", mouseIdValue);
    }
    
    dataFile.printf("%s,%s,%s,%s,%s,%d,%s,",
                    formattedMouseId,
                    sex.c_str(),
                    strain.c_str(),
                    libraryVer,
                    program.c_str(),
                    FR,
                    event.c_str());

    dataFile.printf("%d,%d,%d,%d,", pelletCount, leftCount, rightCount, centerCount);
    dataFile.printf("%d,%d,", blockPelletCount, blockPokeCount);

    // Write motor turns for events where motor has been running
    if (event == "PelletTaken") {
        // Write retrievalTime as string to avoid conversion issues
        if (event == "PelletTaken") {
            if (retrievalTime > 19.9)
            {
                dataFile.print("TimedOut");
            }
            else
            {
                dataFile.printf("%.3f", retrievalTime); // Use printf instead of String conversion
            }
        } else {
            dataFile.print(","); // Empty retrieval time for non-PelletTaken events
        }
        dataFile.write(',');
        dataFile.write(dispenseError ? '1' : '0'); // Write single character
        dataFile.write(',');
        dataFile.print(int(motorTurns/25)); // MotorTurns
        dataFile.write(',');
        // Only reset motorTurns after the final event (PelletTaken)
        if (event == "PelletTaken") {
            motorTurns = 0; // Reset after logging
        }
    }
    else {
        dataFile.print(",,,"); // RetrievalTime, DispenseError
    }

    // Write counters and status
    if (event == "Status" || event == "Startup" || event == "Activity") {
        // Write Activity% (motionPercentage) with 1 decimal place
        dataFile.printf("%.1f,", motionPercentage);

        // Write environmental data
        dataFile.printf("%.1f,%.1f,%.3f,%.3f,",
                        temperature, humidity, lux, white);

        // Write system stats
        dataFile.printf("%d,%d,%d,%d,%.2f,%.2f\n",
                        ESP.getFreeHeap(),
                        ESP.getHeapSize(),
                        ESP.getMinFreeHeap(),
                        wakeCount,
                        cellVoltage,
                        cellPercent);
    } else {
        // Fill empty cells for all data fields when Event is not "Status"
        dataFile.print(",,,,,,,,,,,,,");
        dataFile.println();
    }

    // Clean up
    dataFile.flush();  // Force write to SD card
    dataFile.close();
    digitalWrite(SD_CS, HIGH);
    SPI.endTransaction();
    noPix();
    
    // update screen counters when logging except at startup and not in ActivityMonitor
    if (program != "ActivityMonitor" && (leftCount > 0 || rightCount > 0 || centerCount > 0)) {
      displayIndicators();
      displayCounters();
    }

    refresh();

    return true;
}

/**
 * Retrieves a value from the meta.json configuration file
 *
 * Example usage:
 *   String program = getMetaValue("fed", "program");     // returns "Classic"
 *   String mouseId = getMetaValue("subject", "id");      // returns "mouse001"
 */
String FED4::getMetaValue(const char *rootKey, const char *subKey)
{
    // Check if SD card is available first
    if (!sdCardAvailable) {
        return "";
    }
    
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW); // Select SD card for operation

    File metaFile = SD.open(META_FILE, FILE_READ);
    if (!metaFile)
    {
        digitalWrite(SD_CS, HIGH); // Deselect on error
        // SPI.endTransaction();
        Serial.println("Failed to open meta.json");
        return "";
    }

    const size_t capacity = JSON_OBJECT_SIZE(3) + 120; // Increased for nested objects
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, metaFile);
    metaFile.close();

    digitalWrite(SD_CS, HIGH); // Deselect after operations
    SPI.endTransaction();

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        doc.clear(); // Explicitly clear the document
        return "";
    }

    // Get the root object first
    JsonObject rootObj = doc[rootKey];
    if (!rootObj.isNull())
    {
        // Then get the nested value
        const char *value = rootObj[subKey];
        if (value)
        {
            // Create String only once and return it
            String result(value);
            doc.clear(); // Explicitly clear the document
            return result;
        }
    }

    Serial.printf(" - Value not found for %s > %s\n", rootKey, subKey);
    doc.clear(); // Explicitly clear the document
    return "";
}

/**
 * Sets a value in the meta.json configuration file
 *
 * Example usage:
 *   setMetaValue("subject", "id", "mouse001");     // sets {"subject": {"id": "mouse001"}}
 *   setMetaValue("fed", "program", "Classic");     // sets {"fed": {"program": "Classic"}}
 *
 * @param rootKey The top-level key in the JSON object
 * @param subKey The nested key under rootKey
 * @param value The string value to set
 * @return true if successful, false if failed
 */
bool FED4::setMetaValue(const char *rootKey, const char *subKey, const char *value)
{
    // Check if SD card is available first
    if (!sdCardAvailable) {
        return false;
    }
    
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW); // Select SD card for operation

    // First read existing content
    File metaFile = SD.open(META_FILE, FILE_READ);
    const size_t capacity = JSON_OBJECT_SIZE(3) + 120; // Increased for nested objects
    DynamicJsonDocument doc(capacity);

    if (metaFile)
    {
        DeserializationError error = deserializeJson(doc, metaFile);
        metaFile.close();
        if (error)
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            digitalWrite(SD_CS, HIGH);
            doc.clear(); // Explicitly clear the document
            return false;
        }
    }

    // Get or create the root object while preserving existing content
    JsonObject rootObj;
    if (doc.containsKey(rootKey))
    {
        rootObj = doc[rootKey];
    }
    else
    {
        rootObj = doc.createNestedObject(rootKey);
    }

    // Update only the specified subkey
    rootObj[subKey] = value;

    // Open file for writing
    metaFile = SD.open(META_FILE, FILE_WRITE);
    if (!metaFile)
    {
        digitalWrite(SD_CS, HIGH);
        Serial.println("Failed to open meta.json for writing");
        doc.clear(); // Explicitly clear the document
        return false;
    }

    // Write the updated JSON
    if (serializeJson(doc, metaFile) == 0)
    {
        metaFile.close();
        digitalWrite(SD_CS, HIGH);
        Serial.println("Failed to write to meta.json");
        doc.clear(); // Explicitly clear the document
        return false;
    }

    metaFile.close();
    digitalWrite(SD_CS, HIGH); // Deselect after operations
    doc.clear(); // Explicitly clear the document
    return true;
}

void FED4::setProgram(String program)
{
    setMetaValue("fed", "program", program.c_str());
}

void FED4::setSequenceDisplay(const String& sequence, int index, int level)
{
    currentSequence = sequence;
    currentSequenceIndex = index;
    currentSequenceLevel = level;
}

void FED4::setMouseId(String mouseId)
{
    setMetaValue("subject", "id", mouseId.c_str());
}

void FED4::setSex(String sex)
{
    setMetaValue("subject", "sex", sex.c_str());
}

void FED4::setStrain(String strain)
{
    setMetaValue("subject", "strain", strain.c_str());
}

void FED4::setAge(String age)
{
    setMetaValue("subject", "age", age.c_str());
}

/**
 * Handles SD card errors by playing 2 clicks, blinking red LEDs, 
 * displaying error message, and waiting for Button1 press to continue
 */
void FED4::handleSDCardError()
{
    Serial.println("SD Card Error - Data won't be saved. Continue?");
    
    // Play 2 clicks
    playTone(1000, 8, 0.5);
    delay(100);
    playTone(1000, 8, 0.5);
    delay(100);
    
    // Start blinking red LEDs
    bool ledsOn = true;
    unsigned long lastBlinkTime = 0;
    const unsigned long blinkInterval = 500; // 500ms blink interval
    
    // Clear display and show error message
    clearDisplay();
    
    // Fill the entire display area with black background
    fillRect(0, 0, 144, 168, DISPLAY_BLACK);
    
    setFont(&Org_01);
    setTextSize(2);
    setTextColor(DISPLAY_WHITE); // Use white text on black background
    
    // Display error message with corrected coordinates for 144x168 display
    setCursor(5, 30);
    print("Card error,");
    setCursor(5, 50);
    print("data won't");
    setCursor(5, 70);
    print("be saved.");
    setCursor(5, 100);
    print("Continue?");
    refresh();
    
    // Blink red LEDs and wait for Button1 press
    while (digitalRead(BUTTON_1) == LOW) {
        unsigned long currentTime = millis();
        
        if (currentTime - lastBlinkTime >= blinkInterval) {
            if (ledsOn) {
                colorWipe("red", 0); // Turn all LEDs red
            } else {
                lightsOff(); // Turn all LEDs off
            }
            ledsOn = !ledsOn;
            lastBlinkTime = currentTime;
        }
        
        delay(10); // Small delay to prevent excessive CPU usage
    }
    
    // Button1 was pressed, play highBeep and stop blinking
    lowBeep();
    lightsOff();
    
    // Clear display and show continuing message
    clearDisplay();
    
    // Fill the entire display area with black background
    fillRect(0, 0, 144, 168, DISPLAY_BLACK);
    
    setCursor(5, 80);
    print("Continuing...");
    refresh();
    delay(1000);
    
    Serial.println("User chose to continue without SD card");
}