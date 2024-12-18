#include <FED4.h>

FED4 fed;

void setup()
{
    Serial.begin(115200);
    fed.begin();
    SPI.setBitOrder(MSBFIRST); // only testing SD card
    Serial.println("\nFED4 SD/LDO Test Interface");
    Serial.println("Available commands:");
    Serial.println("1: Release CS pin (INPUT mode)");
    Serial.println("2: Initialize SD card");
    Serial.println("3: Turn off LDO2");
    Serial.println("4: Enter light sleep");
    Serial.println("5: Turn on LDO2");
    Serial.println("6: Set CS pin OUTPUT/HIGH");
    Serial.println("7: Set CS pin LOW");
    Serial.println("8: Send dummy bytes");
    Serial.println("9: Write test file");
    Serial.println("10: End SPI");
    Serial.println("11: Begin SPI");
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
            Serial.println("Releasing CS pin...");
            pinMode(SD_CS, INPUT);
            break;

        case 2:
            Serial.println("Initializing SD card...");
            if (SD.begin(SD_CS, SPI, 4000000))
            {
                Serial.println("SD init success");
            }
            else
            {
                Serial.println("SD init failed");
            }
            break;

        case 3:
            Serial.println("Turning LDO2 off...");
            fed.LDO2_OFF();
            break;

        case 4:
            Serial.println("Entering light sleep...");
            esp_light_sleep_start();
            Serial.println("Woke up!");
            break;

        case 5:
            Serial.println("Turning LDO2 on...");
            fed.LDO2_ON();
            break;

        case 6:
            Serial.println("Setting CS pin OUTPUT/HIGH...");
            pinMode(SD_CS, OUTPUT);
            digitalWrite(SD_CS, HIGH);
            break;

        case 7:
            Serial.println("Setting CS pin LOW...");
            digitalWrite(SD_CS, LOW);
            break;

        case 8:
            Serial.println("Sending dummy bytes...");
            for (int i = 0; i < 10; i++)
            {
                uint8_t received = SPI.transfer(0xFF);
                Serial.printf("Byte %d: 0x%02X\n", i, received);
            }
            break;

        case 9:
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

        case 10:
            Serial.println("Ending SPI...");
            SPI.end();
            break;

        case 11:
            Serial.println("Beginning SPI...");
            SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
            break;

        default:
            Serial.println("Unknown command");
            break;
        }
    }
}
