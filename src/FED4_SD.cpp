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

    // Try different SD card initialization speeds
    for (uint8_t i = 0; i < 3; i++)
    {
        digitalWrite(SD_CS, LOW); // Select SD card (active LOW)
        if (SD.begin(SD_CS, SPI, 4000000))
        {
            digitalWrite(SD_CS, HIGH); // Deselect SD card
            Serial.println("SD card initialized successfully");
            return true;
        }
        digitalWrite(SD_CS, HIGH); // Ensure SD is deselected on failure
        delay(100);
    }
    Serial.println("SD card initialization failed");
    return false;
}

/**
 * Creates a new log file with headers
 */
void FED4::createLogFile()
{
    DateTime now = rtc.now();
    String id = mouseId.length() > 0 ? mouseId : "nan";
    char baseFilename[50];
    int fileNumber = 0;

    // Keep trying filenames with incrementing numbers until we find one that doesn't exist
    do {
        snprintf(baseFilename, sizeof(baseFilename), "/FED4_%s_%04d%02d%02d_%02d.CSV", 
                id.c_str(), now.year(), now.month(), now.day(), fileNumber);
        Serial.print("Trying filename: ");
        Serial.println(baseFilename);
        fileNumber++;
    } while (SD.exists(baseFilename) && fileNumber < 100);

    // Copy final filename to class member - ensure null termination
    strncpy(filename, baseFilename, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    
    // Just change bit order for SD operations
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW);

    // Create new file and write headers
    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile)
    {
        dataFile.print("DateTime,ElapsedSeconds,Event,PelletCount,LeftCount,RightCount,CenterCount,RetrievalTime,DispenseError,");
        dataFile.println("Temperature,Humidity,Lux,FreeHeap,HeapSize,MinFreeHeap,WakeCount,DispenseTurns,BatteryVoltage,BatteryPercent");
        dataFile.close();
        Serial.println("Created new data file with headers");
    }

    Serial.print("New file created: ");
    Serial.println(filename);

    digitalWrite(SD_CS, HIGH); // Deselect after operations
}

/**
 * Logs data to the SD card
 * @param newEvent the event to log
 */
void FED4::logData(const String &newEvent)
{
    // Set new event if provided
    if (newEvent.length() > 0)
    {
        setEvent(newEvent);
    }

    File dataFile;
    SPI.setBitOrder(MSBFIRST);
    greenPix();

    DateTime now = rtc.now();
    float currentSeconds = millis()/1000.000; // Get current seconds

    // Open file for writing
    digitalWrite(SD_CS, LOW); // Select SD card for operation
    dataFile = SD.open(filename, FILE_APPEND);
    if (!dataFile)
    {
        digitalWrite(SD_CS, HIGH);
        Serial.print("Failed to open file: ");
        Serial.println(filename);
        noPix();
        return;
    }

    // Write all data
    dataFile.printf("%04d-%02d-%02d %02d:%02d:%02d,%f,%s,",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second(),
                    currentSeconds,
                    event.c_str());
                
    dataFile.printf("%d,%d,%d,%d,%d,%d,",
                    pelletCount, leftCount, rightCount, centerCount, retrievalTime, dispenseError);

    dataFile.printf("%.1f,%.1f,%.1f,",
                    temperature, humidity, lux);

    dataFile.printf("%d,%d,%d,",
                    ESP.getFreeHeap(),
                    ESP.getHeapSize(),
                    ESP.getMinFreeHeap());

    dataFile.printf("%d,%d,%.1f,%.1f\n",
                    wakeCount, (int)motorTurns/125, cellVoltage, cellPercent);  //it takes about 125 motorTurns to move one pellet position

    Serial.print("Data logged to: ");
    Serial.println(filename);

    // Clean up
    dataFile.close();
    digitalWrite(SD_CS, HIGH);
    SPI.endTransaction();
    noPix();
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
            return String(value);
        }
    }

    Serial.printf("Value not found for %s > %s\n", rootKey, subKey);
    return "";
}
