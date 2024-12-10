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
    char filename[20];
    sprintf(filename, "/%04d%02d%02d.CSV", now.year(), now.month(), now.day());

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

void FED4::logData(const char *event)
{
    DateTime now = rtc.now();
    char filename[20];
    sprintf(filename, "/%04d%02d%02d.CSV", now.year(), now.month(), now.day());

    File dataFile = SD.open(filename, FILE_APPEND);
    if (dataFile)
    {
        // DateTime, Event
        dataFile.printf("%04d-%02d-%02d %02d:%02d:%02d,%s,",
                        now.year(), now.month(), now.day(),
                        now.hour(), now.minute(), now.second(),
                        event);

        // Counters
        dataFile.printf("%d,%d,%d,%d,%d,",
                        pelletCount, leftCount, rightCount, centerCount, wakeCount);

        // Battery and Environmental
        dataFile.printf("%.2f,%.1f,",
                        maxlipo.cellVoltage(),
                        aht.getTemperature());

        // Memory stats
        dataFile.printf("%d,%d,%d\n",
                        ESP.getFreeHeap(),
                        ESP.getHeapSize(),
                        ESP.getMinFreeHeap());

        dataFile.close();
    }
}

// ... other functions ...