#include "FED4.h"

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 if writing, 0x00 if reading
#define SHARPMEM_BIT_VCOM (0x02)     // ROM_IN pin state
#define SHARPMEM_BIT_CLEAR (0x04)    // CL pin state
#define SHARPMEM_SPI_FREQ (1000000)  // 1 MHz SPI clock frequency

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

// Add these lookup tables at the top of the file
static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1, (uint8_t)~2, (uint8_t)~4,
                                      (uint8_t)~8, (uint8_t)~16, (uint8_t)~32,
                                      (uint8_t)~64, (uint8_t)~128};

void FED4::updateDisplay()
{
    // Step 1: Initial setup
    clearDisplay();
    setRotation(2);
    setTextColor(DISPLAY_BLACK);

    // Step 2: Title
    setTextSize(3);
    setCursor(12, 20);
    print("FED4");
    refresh();

    // Step 3: Status information
    setTextSize(1);

    // Counts
    setCursor(12, 56);
    print("Pellets: ");
    print(pelletCount);
    refresh();

    setCursor(12, 72);
    print("L:");
    print(leftCount);
    print("   C:");
    print(centerCount);
    print("   R:");
    print(rightCount);
    refresh();

    // Environmental
    setCursor(12, 90);
    print("Temp: ");
    print(getTemperature(), 1);
    print("C");
    print(" Hum: ");
    print(getHumidity(), 1);
    println("%");
    refresh();

    // Battery
    float cellVoltage = getBatteryVoltage();
    float cellPercent = getBatteryPercentage();
    setCursor(12, 122);
    print("Fuel: ");
    print(cellVoltage, 1);
    print("V, ");
    print(cellPercent, 1);
    println("%");
    refresh();

    // Time
    DateTime currentTime = now();
    setCursor(12, 140);
    print(currentTime.month());
    print("/");
    print(currentTime.day());
    print("/");
    print(currentTime.year());
    print(" ");
    print(currentTime.hour());
    print(":");
    if (currentTime.minute() < 10)
        print('0');
    print(currentTime.minute());
    refresh();

    // Wake count
    setCursor(12, 156);
    print("Unclear:");
    print(wakeCount);
    refresh();
}

void FED4::serialStatusReport()
{
    // Print header
    Serial.println(F("\n================================================"));
    Serial.println(F("                 FED4 Status Report               "));
    Serial.println(F("================================================"));

    // Date and Time section
    Serial.print(F("\n► DateTime: "));
    serialPrintRTC();
    Serial.println();

    // Environmental Data section
    Serial.println(F("\n► Environmental Data"));
    Serial.print(F("  • Temperature:  "));
    Serial.print(getTemperature(), 1);
    Serial.println(F(" °C"));
    Serial.print(F("  • Humidity:     "));
    Serial.print(getHumidity(), 1);
    Serial.println(F("%"));

    // Battery Status section
    Serial.println(F("\n► Battery Status"));
    if (isBatteryConnected())
    {
        Serial.print(F("  • Voltage: "));
        Serial.print(getBatteryVoltage(), 1);
        Serial.println(F("V"));
        Serial.print(F("  • Charge:  "));
        Serial.print(getBatteryPercentage(), 1);
        Serial.println(F("%"));
    }
    else
    {
        Serial.println(F("  • USB Powered"));
    }

    // Pellet counts section
    Serial.println(F("\n► Pellet Count"));
    Serial.print(F("  • Total: "));
    Serial.println(pelletCount);

    Serial.println(F("\n► Pokes"));
    Serial.print(F("  • Left: "));
    Serial.print(leftCount);
    Serial.print(F(" | Center: "));
    Serial.print(centerCount);
    Serial.print(F(" | Right: "));
    Serial.println(rightCount);

    // Memory Statistics section
    Serial.println(F("\n► Memory Statistics"));
    Serial.print(F("  • Free Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    Serial.print(F("  • Heap Size: "));
    Serial.print(ESP.getHeapSize());
    Serial.println(F(" bytes"));
    Serial.print(F("  • Min Free Heap: "));
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(F(" bytes"));

    Serial.println(F("\n================================================\n"));
}

void FED4::initializeDisplay()
{
    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(DISPLAY_CS, LOW); // Display inactive = LOW
    SPI.setBitOrder(LSBFIRST);

    // Initialize the display buffer
    if (displayBuffer)
    {
        free(displayBuffer);
    }
    uint16_t bufferSize = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8;
    displayBuffer = (uint8_t *)malloc(bufferSize);
    if (!displayBuffer)
    {
        Serial.println("Failed to allocate display buffer");
        return;
    }
    memset(displayBuffer, 0xff, bufferSize);

    vcom = false;

    // Initialize display with a clear command
    SPI.setBitOrder(LSBFIRST);
    digitalWrite(DISPLAY_CS, HIGH); // Select display

    // Send clear command
    uint8_t cmd = SHARPMEM_BIT_WRITECMD | SHARPMEM_BIT_CLEAR;
    if (vcom)
        cmd |= SHARPMEM_BIT_VCOM;
    vcom = !vcom;

    SPI.transfer(cmd);
    SPI.transfer(0x00); // Required trailing byte

    digitalWrite(DISPLAY_CS, LOW); // Deselect display

    delay(10); // Give display time to process clear command

    // Set initial display state
    setRotation(2);
    refresh(); // Initial refresh to ensure display is ready
}

void FED4::sendDisplayCommand(uint8_t cmd)
{
    SPI.setBitOrder(LSBFIRST);
    digitalWrite(DISPLAY_CS, HIGH); // Select display (active HIGH)

    // Toggle VCOM
    cmd |= SHARPMEM_BIT_WRITECMD;
    if (vcom)
    {
        cmd |= SHARPMEM_BIT_VCOM;
    }
    vcom = !vcom;

    SPI.transfer(cmd);

    digitalWrite(DISPLAY_CS, LOW); // Deselect display
}

void FED4::clearDisplay()
{
    sendDisplayCommand(SHARPMEM_BIT_CLEAR);
    delay(1); // Wait for the clear to complete
    memset(displayBuffer, 0xff, (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8);
}

void FED4::refresh()
{
    uint16_t i, currentline;

    SPI.setBitOrder(LSBFIRST);
    digitalWrite(DISPLAY_CS, HIGH);

    uint8_t cmd = SHARPMEM_BIT_WRITECMD;
    if (vcom)
    {
        cmd |= SHARPMEM_BIT_VCOM;
    }
    vcom = !vcom;

    SPI.transfer(cmd);

    uint8_t bytes_per_line = DISPLAY_WIDTH / 8;
    uint16_t totalbytes = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8;

    // Send all lines
    for (i = 0; i < totalbytes; i += bytes_per_line)
    {
        // Prepare the line buffer
        uint8_t line[bytes_per_line + 2];

        // Send address byte (line number)
        currentline = ((i + 1) / (DISPLAY_WIDTH / 8)) + 1;
        line[0] = currentline;

        // Copy display data for this line
        memcpy(line + 1, displayBuffer + i, bytes_per_line);

        // Add end of line marker
        line[bytes_per_line + 1] = 0x00;

        // Send the entire line at once
        for (uint8_t j = 0; j < bytes_per_line + 2; j++)
        {
            SPI.transfer(line[j]);
        }
    }

    SPI.transfer(0x00);

    digitalWrite(DISPLAY_CS, LOW);
}

void FED4::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (x >= DISPLAY_WIDTH) || (y < 0) || (y >= DISPLAY_HEIGHT))
        return;

    switch (rotation)
    {
    case 1:
        _swap_int16_t(x, y);
        x = DISPLAY_WIDTH - 1 - x;
        break;
    case 2:
        x = DISPLAY_WIDTH - 1 - x;
        y = DISPLAY_HEIGHT - 1 - y;
        break;
    case 3:
        _swap_int16_t(x, y);
        y = DISPLAY_HEIGHT - 1 - y;
        break;
    }

    if (color)
    {
        displayBuffer[(y * DISPLAY_WIDTH + x) / 8] |= pgm_read_byte(&set[x & 7]);
    }
    else
    {
        displayBuffer[(y * DISPLAY_WIDTH + x) / 8] &= pgm_read_byte(&clr[x & 7]);
    }
}
