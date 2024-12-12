#include "FED4.h"

/********************************************************
 * SD Card Functions
 ********************************************************/

bool FED4::initializeSD()
{
    // Initialize SPI for SD card
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);

    // Try different SD card initialization speeds
    for (uint8_t i = 0; i < 3; i++)
    {
        if (SD.begin(SD_CS, SPI, 4000000))
        { // Try 4MHz first
            Serial.println("SD card initialized successfully");
            return true;
        }
        else if (SD.begin(SD_CS, SPI, 1000000))
        { // Try slower 1MHz
            Serial.println("SD card initialized at lower speed");
            return true;
        }
        else
        {
            Serial.println("SD initialization attempt " + String(i + 1) + " failed");
            delay(100);
        }
    }

    if (!SD.cardType())
    {
        Serial.println("No SD card attached");
    }
    return false;
}

void FED4::createLogFile()
{
    // Create a new file with a unique name based on date/time
    DateTime now = rtc.now();
    sprintf(filename, "/%04d%02d%02d.CSV", now.year(), now.month(), now.day());
    Serial.print ("New file created: ");
    Serial.println (filename);

    // Check if file exists, if not create it and write headers
    if (!SD.exists(filename))
    {
        File dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
            dataFile.println("DateTime,Event,PelletCount,LeftCount,RightCount,CenterCount,WakeCount,"
                             "Battery,Temperature,FreeHeap,HeapSize,MinFreeHeap");
            dataFile.close();
            Serial.println("Created new data file with headers");
        }
    }
}

void FED4::logData()
{
    greenPix();
    
    Serial.println ("Getting time");
    DateTime now = rtc.now();
    
    Serial.print ("Attempting to open file: ");
    Serial.print (filename);
    File dataFile = SD.open(filename, FILE_WRITE);

    if (dataFile)
    {
    Serial.println (" ... opened");
        // DateTime, Event
        dataFile.printf("%04d-%02d-%02d %02d:%02d:%02d,%s,",
                        now.year(), now.month(), now.day(),
                        now.hour(), now.minute(), now.second(),
                        event.c_str());

        // Counters
        dataFile.printf("%d,%d,%d,%d,%d,",
                        pelletCount, leftCount, rightCount, centerCount, wakeCount);

        // Battery and Environmental
        dataFile.printf("%.2f,%.1f,",
                        getBatteryVoltage(),
                        getTemperature());

        // Memory stats
        dataFile.printf("%d,%d,%d\n",
                        ESP.getFreeHeap(),
                        ESP.getHeapSize(),
                        ESP.getMinFreeHeap());

        dataFile.close();
    }
    
    else {
    Serial.println(" ... Warning: SD file not opened.");
    }
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
    File metaFile = SD.open(META_FILE, FILE_READ);
    if (!metaFile)
    {
        Serial.println("Failed to open meta.json");
        return "";
    }

    const size_t capacity = JSON_OBJECT_SIZE(3) + 120; // Increased for nested objects
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, metaFile);
    metaFile.close();

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
