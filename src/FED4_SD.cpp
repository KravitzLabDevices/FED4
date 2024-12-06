#include "FED4.h"

/********************************************************
 * SD Card Functions
 ********************************************************/

bool FED4::initializeSD()
{
    // Initialize SD card
    if (!SD.begin(SD_CS))
    {
        Serial.println("SD card initialization failed!");
        return false;
    }
    Serial.println("SD card initialized successfully.");

    // Check if we can write to the SD card
    File testFile = SD.open("/test.txt", FILE_WRITE);
    if (testFile)
    {
        testFile.println("SD Card Write Test");
        testFile.close();
        Serial.println("SD card write test successful.");
        return true;
    }
    else
    {
        Serial.println("Error opening test file!");
        return false;
    }
}

void FED4::createDataFile()
{
    // Create a new file with a unique name based on date/time
    DateTime now = rtc.now();
    char filename[20];
    sprintf(filename, "/%04d%02d%02d.CSV", now.year(), now.month(), now.day());

    // Check if file exists, if not create it and write headers
    if (!SD.exists(filename))
    {
        File dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
            dataFile.println("DateTime,Event,PelletCount,LeftCount,RightCount,CenterCount,Battery,Temperature");
            dataFile.close();
            Serial.println("Created new data file with headers");
        }
    }
}

// ... other functions ...