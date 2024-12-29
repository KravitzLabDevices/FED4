#include "FED4.h"

// This chip does not like SPI.beginTransaction(SPISettings(SHARPMEM_SPI_FREQ, LSBFIRST, SPI_MODE0));
// SPI will stop working. The display can operate on SPI_MODE0, just LSBFIRST.
// So we need to set the bit order to MSBFIRST for SD operations.

/********************************************************
 * SD Card Functions
 ********************************************************/

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

void FED4::createLogFile()
{
    DateTime now = rtc.now();
    sprintf(filename, "/%04d%02d%02d.CSV", now.year(), now.month(), now.day());

    // Just change bit order for SD operations
    SPI.setBitOrder(MSBFIRST);
    digitalWrite(SD_CS, LOW);

    // Check if file exists, if not create it and write headers
    if (!SD.exists(filename))
    {
        File dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
            dataFile.println("DateTime,Millis,Event,PelletCount,LeftCount,RightCount,CenterCount,WakeCount,"
                             "Battery,Temperature,FreeHeap,HeapSize,MinFreeHeap");
            dataFile.close();
            Serial.println("Created new data file with headers");
        }
    }

    Serial.print("New file created: ");
    Serial.println(filename);

    digitalWrite(SD_CS, HIGH); // Deselect after operations
}

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
    unsigned long currentMillis = millis(); // Get current milliseconds

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
    dataFile.printf("%04d-%02d-%02d %02d:%02d:%02d,%lu,%s,",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second(),
                    currentMillis,
                    event.c_str());

    dataFile.printf("%d,%d,%d,%d,%d,",
                    pelletCount, leftCount, rightCount, centerCount, wakeCount);

    dataFile.printf("%.2f,%.1f,",
                    cellVoltage,
                    temperature);

    dataFile.printf("%d,%d,%d\n",
                    ESP.getFreeHeap(),
                    ESP.getHeapSize(),
                    ESP.getMinFreeHeap());

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
