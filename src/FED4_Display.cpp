#include "FED4.h"

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 if writing, 0x00 if reading
#define SHARPMEM_BIT_VCOM (0x02)     // ROM_IN pin state
#define SHARPMEM_BIT_CLEAR (0x04)    // CL pin state

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

// void FED4::updateDisplay()
// {
//     // Step 1: Initial setup
//     clearDisplay();
//     setRotation(2);
//     setTextColor(DISPLAY_BLACK);

//     // Step 2: Title
//     setTextSize(3);
//     setCursor(12, 20);
//     print("FED4");
//     refresh();

//     // Step 3: Status information
//     setTextSize(1);

//     // Counts
//     setCursor(12, 56);
//     print("Pellets: ");
//     print(pelletCount);
//     refresh();

//     setCursor(12, 72);
//     print("L:");
//     print(leftCount);
//     print("   C:");
//     print(centerCount);
//     print("   R:");
//     print(rightCount);
//     refresh();

//     // Environmental
//     setCursor(12, 90);
//     print("Temp: ");
//     print(getTemperature(), 1);
//     print("C");
//     print(" Hum: ");
//     print(getHumidity(), 1);
//     println("%");
//     refresh();

//     // Battery
//     float cellVoltage = getBatteryVoltage();
//     float cellPercent = getBatteryPercentage();
//     setCursor(12, 122);
//     print("Fuel: ");
//     print(cellVoltage, 1);
//     print("V, ");
//     print(cellPercent, 1);
//     println("%");
//     refresh();

//     // Time
//     DateTime currentTime = now();
//     setCursor(12, 140);
//     print(currentTime.month());
//     print("/");
//     print(currentTime.day());
//     print("/");
//     print(currentTime.year());
//     print(" ");
//     print(currentTime.hour());
//     print(":");
//     if (currentTime.minute() < 10)
//         print('0');
//     print(currentTime.minute());
//     refresh();

//     // Wake count
//     setCursor(12, 156);
//     print("Unclear:");
//     print(wakeCount);
//     refresh();
// }


void FED4::updateDisplay() {
  clearDisplay();

  //Box around data area of screen
  drawRect (5, 56, 134, 86, DISPLAY_BLACK);
  
  setTextSize(2);
  setFont(&Org_01);
 
  setCursor(5, 13);
  print("FED4");
  setCursor(6, 13);  // this doubling is a way to do bold type
  print("FED4");
//  fillRect (6, 20, 200, 22, WHITE);  //erase text under battery row without clearing the entire screen
//  fillRect (35, 46, 120, 68, WHITE);  //erase the pellet data on screen without clearing the entire screen 
//   setCursor(5, 36); //display which sketch is running
  
  //write the first 8 characters of sessiontype:
//   print(sessiontype.charAt(0));
//   print(sessiontype.charAt(1));
//   print(sessiontype.charAt(2));
//   print(sessiontype.charAt(3));
//   print(sessiontype.charAt(4));
//   print(sessiontype.charAt(5));
//   print(sessiontype.charAt(6));
//   print(sessiontype.charAt(7));

  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setCursor(30, 75);
  print("Left: ");
  setCursor(90, 75);
  print(leftCount);
  setCursor(30, 95);
  print("Center: ");
  setCursor(90, 95);
  print(centerCount);
  setCursor(30, 115);
  print("Right:  ");
  setCursor(90, 115);
  print(rightCount);
  setCursor(30, 135);
  print("Pellets:");
  setCursor(90, 135);
  print(pelletCount);
    
  displayBattery();
  displayDateTime();
  displayIndicators();
  displayEnvironmental();
  refresh();
}

void FED4::displayDateTime(){
  DateTime current = rtc.now();

  // Print date and time at bottom of the screen
  setCursor(0, 160);
  //  fillRect (0, 123, 200, 60, WHITE);
  print(current.month());
  print("/");
  print(current.day());
  print("/");
  print(current.year()-2000);
  print("     ");
  if (current.hour() < 10)
    print(' ');      // Trick to add leading zero for formatting
  print(current.hour());
  print(":");
  if (current.minute() < 10)
    print('0');      // Trick to add leading zero for formatting
  print(current.minute());
}

void FED4::displayIndicators(){
  //Left 
  fillCircle(17, 70, 5, DISPLAY_WHITE); 
  drawCircle(17, 70, 5, DISPLAY_BLACK);

  //Center
  fillCircle(17, 90, 5, DISPLAY_WHITE); 
  drawCircle(17, 90, 5, DISPLAY_BLACK);

  //Right
  fillCircle(17, 110, 5, DISPLAY_WHITE);
  drawCircle(17, 110, 5, DISPLAY_BLACK);

  //Pellets 
  fillCircle(17, 130, 5, DISPLAY_WHITE);
  drawCircle(17, 130, 5, DISPLAY_BLACK);

  //add triangles to indicate which poke is active
  // TO DO
}

void FED4::displayBattery(){
  float cellVoltage = getBatteryVoltage();
  float cellPercent = getBatteryPercentage();

  //  Battery graphic showing bars indicating voltage levels
//   display.fillRect (117, 2, 40, 16, WHITE);
//   display.drawRect (116, 1, 42, 18, BLACK);
//   display.drawRect (157, 6, 6, 8, BLACK);
  
//   //4 bars
//   if (cellVoltage > 3.85) {
//     fillRect (120, 4, 7, 12, BLACK);
//     fillRect (129, 4, 7, 12, BLACK);
//     fillRect (138, 4, 7, 12, BLACK);
//     fillRect (147, 4, 7, 12, BLACK);
//   }

//   //3 bars
//   else if (cellVoltage > 3.7) {
//     fillRect (119, 3, 26, 13, WHITE);
//     fillRect (120, 4, 7, 12, BLACK);
//     fillRect (129, 4, 7, 12, BLACK);
//     fillRect (138, 4, 7, 12, BLACK);
//   }

//   //2 bars
//   else if (cellVoltage > 3.55) {
//     fillRect (119, 3, 26, 13, WHITE);
//     fillRect (120, 4, 7, 12, BLACK);
//     fillRect (129, 4, 7, 12, BLACK);
//   }

//   //1 bar
//   else {
//     fillRect (119, 3, 26, 13, WHITE);
//     fillRect (120, 4, 7, 12, BLACK);
//   }
  
  //display voltage
//  fillRect (86, 0, 28, 12, WHITE);
  setTextSize(2);
  setFont(&Org_01);
  setCursor(85, 13);
  print(cellVoltage, 1);
  print("V");
  setTextSize(1);
  setFont(&FreeSans9pt7b);
}

void FED4::displayEnvironmental(){
  setFont(&Org_01);
  setTextSize(2);
  setCursor(85, 28);
  print(getTemperature(), 1);
  print("C, ");
  setCursor(85, 43);
  print(getHumidity(), 1);
  print("%");
  setFont(&FreeSans9pt7b);
  setTextSize(1);
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
    Serial.print(F("  • Voltage: "));
    Serial.print(getBatteryVoltage(), 1);
    Serial.println(F("V"));
    Serial.print(F("  • Charge:  "));
    Serial.print(getBatteryPercentage(), 1);
    Serial.println(F("%"));

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

bool FED4::initializeDisplay()
{
    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(DISPLAY_CS, LOW); // Display inactive = LOW
    SPI.setBitOrder(LSBFIRST);

    // Initialize the display buffer
    if (displayBuffer)
    {
        free(displayBuffer);
        displayBuffer = nullptr;
    }
    uint16_t bufferSize = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8;
    displayBuffer = (uint8_t *)malloc(bufferSize);
    if (!displayBuffer)
    {
        Serial.println("Failed to allocate display buffer");
        return false;
    }
    memset(displayBuffer, 0xff, bufferSize);

    vcom = false;

    // Initialize display with a clear command
    SPI.setBitOrder(LSBFIRST);
    digitalWrite(DISPLAY_CS, HIGH); // Select display

    uint8_t cmd = SHARPMEM_BIT_WRITECMD | SHARPMEM_BIT_CLEAR;
    if (vcom)
        cmd |= SHARPMEM_BIT_VCOM;
    vcom = !vcom;

    SPI.transfer(cmd);
    SPI.transfer(0x00); // Required trailing byte

    digitalWrite(DISPLAY_CS, LOW); // Deselect display

    delay(10); // Give display time to process clear command

    setRotation(2);
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_BLACK);
    setTextWrap(false);

    refresh(); // Initial refresh to ensure display is ready
    return true;
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

void FED4::startupAnimation(){
  setTextSize(5);
  setFont(&Org_01);

  const char* text = "FED4";  // Text to animate
  int textWidth = 28;         // Approximate width of each character in pixels
  int textX = 144;   // Start position off the screen (right side)
  int mouseX = 0;
  int centerX = (144 - strlen(text) * textWidth) / 2; // Center X position
  int textY = (168 - 32) / 2; // Vertical center for the text

  while (textX > centerX) {
    clearDisplay();

    //draw FED4
    fillRect(110, 102, 32, 20, DISPLAY_BLACK);    //FED4
    fillRect(122, 92, 16, 8, DISPLAY_BLACK);     //hopper
    fillCircle(118, 108, 3, DISPLAY_WHITE);       //poke 1
    fillCircle(134, 108, 3, DISPLAY_WHITE);       //poke 2
    fillCircle(126, 114, 2, DISPLAY_WHITE);       //poke 3

    // Draw the text sliding in from the right
    setCursor(textX, textY);
    print(text);
    setCursor(textX+1, textY);
    print(text);

    // Move the text to the left
    textX -= 2;  // Adjust the speed by changing this value
    mouseX += 1; // Adjust the speed of the mouse

    fillRoundRect (mouseX + 25, 92, 15, 10, 7, DISPLAY_BLACK);    //head
    fillRoundRect (mouseX + 22, 90, 8, 5, 3, DISPLAY_BLACK);      //ear
    fillRoundRect (mouseX + 30, 94, 1, 1, 1, DISPLAY_WHITE);      //eye

    //movement of the mouse
    if ((mouseX  / 10) % 2 == 0) {
      fillRoundRect (mouseX, 94, 32, 17, 10, DISPLAY_BLACK);      //body
      drawFastHLine(mouseX  - 8, 95, 18, DISPLAY_BLACK);           //tail
      drawFastHLine(mouseX  - 8, 96, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX  - 14, 94, 8, DISPLAY_BLACK);
      drawFastHLine(mouseX  - 14, 95, 8, DISPLAY_BLACK);
      fillRoundRect (mouseX  + 22, 109, 8, 4, 3, DISPLAY_BLACK);    //front foot
      fillRoundRect (mouseX  , 107, 8, 6, 3, DISPLAY_BLACK);        //back foot
    }
    else {
      fillRoundRect (mouseX + 2, 92, 30, 17, 10, DISPLAY_BLACK);  //body
      drawFastHLine(mouseX - 6, 101, 18, DISPLAY_BLACK);            //tail
      drawFastHLine(mouseX - 6, 100, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX - 12, 102, 8, DISPLAY_BLACK);
      drawFastHLine(mouseX - 12, 101, 8, DISPLAY_BLACK);
      fillRoundRect (mouseX  + 15, 109, 8, 4, 3, DISPLAY_BLACK);    //foot
      fillRoundRect (mouseX + 8, 107, 8, 6, 3, DISPLAY_BLACK);      //back foot
    }
    // Update the display
    refresh();
    delay(30);   // Adjust the frame rate by changing this delay
  }

  // Display the text in the center and hold
  setCursor(textX, textY);
  setCursor(textX+1, textY);
  print(text);
  refresh();
  setTextSize(1);
  delay(2000); // Pause to keep "FED4" displayed for 2s
}