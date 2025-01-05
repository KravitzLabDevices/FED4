#include <FED4.h>

FED4 fed;

void setup()
{
    Serial.begin(115200);
    fed.begin();
    SPI.setBitOrder(MSBFIRST); // only testing SD card

    Serial.println("\nFED4 SD/LDO Test Interface");
    Serial.println("Available commands:");
    Serial.println("\nInitial SD Card Setup:");
    Serial.println("1: Begin SPI");
    Serial.println("2: Initialize SD card");
    Serial.println("3: Write test file");

    Serial.println("\nEntering Sleep:");
    Serial.println("4: Release CS pin (INPUT mode)");
    Serial.println("5: Turn off LDO2");
    Serial.println("6: Enter light sleep");

    Serial.println("\nWaking Up:");
    Serial.println("7: Turn on LDO2");
    Serial.println("8: Set CS pin OUTPUT/HIGH");
    Serial.println("9: Send dummy bytes");
    Serial.println("10: Write test file");

    Serial.println("\nUtilities:");
    Serial.println("11: Set CS pin LOW");
    Serial.println("12: End SPI");
    Serial.println("13: End SD");
}

void loop()
{
    if (Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        switch (cmd.toInt())
        {
        case 1:
            Serial.println("Beginning SPI...");
            SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
            break;

        case 2:
            Serial.println("Initializing SD card...");
            if (SD.begin(SD_CS, SPI, 1000000))
            {
                Serial.println("SD init success");
            }
            else
            {
                Serial.println("SD init failed");
            }
            break;

        case 3:
        case 10:
            Serial.println("Writing test file...");
            {
                File dataFile = SD.open("/test.csv", FILE_WRITE);
                if (dataFile)
                {
                    dataFile.print(0x1F);
                    dataFile.close();
                    Serial.println("Write successful");
                }
                else
                {
                    Serial.println("File open failed");
                }
            }
            break;

        case 4:
            Serial.println("Releasing CS pin...");
            pinMode(SD_CS, INPUT);
            break;

        case 5:
            Serial.println("Turning LDO2 off...");
            fed.LDO2_OFF();
            break;

        case 6:
            Serial.println("Entering light sleep...");
            esp_light_sleep_start();
            Serial.println("Woke up!");
            break;

        case 7:
            Serial.println("Turning LDO2 on...");
            fed.LDO2_ON();
            break;

        case 8:
            Serial.println("Setting CS pin OUTPUT/HIGH...");
            pinMode(SD_CS, OUTPUT);
            digitalWrite(SD_CS, HIGH);
            break;

        case 9:
            Serial.println("Sending dummy bytes...");
            for (int i = 0; i < 10; i++)
            {
                uint8_t received = SPI.transfer(0xFF);
                Serial.printf("Byte %d: 0x%02X\n", i, received);
            }
            break;

        case 11:
            Serial.println("Setting CS pin LOW...");
            digitalWrite(SD_CS, LOW);
            break;

        case 12:
            Serial.println("Ending SPI...");
            SPI.end();
            break;

        case 13:
            Serial.println("Ending SD...");
            SD.end();
            break;

        default:
            Serial.println("Unknown command");
            break;
        }
    }
}
