#include "FED4.h"

// Display: Kyocera TN0216ANVNANN-GN00  320×176 Memory-in-Pixel (MIP)
// Interface: 3-wire SPI (SCLK, SCS, SI) + RST + VCOM
// RST = LOW → display ON;  RST = HIGH → display OFF (VCOM must be LOW when RST HIGH)
// Pixel data: 0 = BLACK, 1 = WHITE  (section 9-1)
// SPI bit order: LSBFIRST — AG0 / D0 are the "first" bits per the datasheet notation,
//   meaning they map to bit 0 (transmitted first by LSBFIRST).
// Gate address: linear mapping assumed (address = line number 1–176).
//   If rows appear interleaved, consult the section 7 address table in the datasheet.

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

// Pixel bit-mask lookup tables (LSBFIRST: bit 0 = leftmost pixel in each byte group)
static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

void FED4::updateDisplay() {
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  // Check if this is ActivityMonitor program
  if (program == "ActivityMonitor") {
    displayActivityMonitor();
  } else {
    displayTask();
    displayMouseId();

    drawLine(0, 60, 175, 60, DISPLAY_BLACK);  

    // draw screen elements
    displayEnvironmental();
    displayBattery();
    displaySDCardStatus();
    displayCounters();
    displayIndicators();
    displayDateTime();
  }
  refresh();
}

void FED4::displayActivityMonitor() {
  // Use the same layout as normal FED4 display but replace counters and indicators
  displayTask();
  displayMouseId();

  drawLine(0, 60, 175, 60, DISPLAY_BLACK);  

  // draw screen elements (same as normal display)
  displayEnvironmental();
  displayBattery();
  displaySDCardStatus();
  
  // Replace displayCounters() with activity information
  displayActivityCounters();

  displayDateTime();
}

void FED4::displayActivityCounters() {
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  
  // Clear all counter value areas with one white rectangle (same as displayCounters)
  fillRect(90, 68, 50, 78, DISPLAY_WHITE);  // Clear area for all counter values
  
  setCursor(6, 80);
  print("Activity ");
  setCursor(90, 80);
  print(motionCount);
  
  setCursor(6, 100);
  print("Activity% ");
  setCursor(90, 100);
  
  // Display motion percentage (calculated in real-time by motion() and pollSensors())
  printf("%.1f", motionPercentage);
  
  setCursor(6, 120);
  print("Seconds");
  setCursor(90, 120);

  // Initialize pollSensorsTimer if it hasn't been set yet
  if (pollSensorsTimer == 0) {
    pollSensorsTimer = millis();
  }

  //print elapsed seconds since pollSensorsTimer was reset
  print((millis() - pollSensorsTimer) / 1000);
  
  setCursor(6, 140);
  print("Uptime(h)");
  setCursor(90, 140);
  // Calculate total uptime in hours with 2 decimal places
  float uptimeHours = millis() / 1000.0 / 3600.0;
  printf("%.2f", uptimeHours);
}

void FED4::displayTask() {
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  
  if (program == "SequenceLearning") {
    // Display sequence information
    setCursor(6, 35);
    print("Seq:");
    
    // Clear area for sequence display
    fillRect(40, 35, 120, 30, DISPLAY_WHITE);
    
    if (currentSequence.length() > 0) {
      setCursor(50, 35);
      
      // Display each character in the sequence (show entire required sequence)
      for (int i = 0; i < currentSequence.length(); i++) {
        char c = currentSequence[i];
        
        // Show the entire required sequence for the current level
        if (i < currentSequenceLevel) {
          // Required sequence items - all with black background, white text
          fillRect(43 + (i * 12), 20, 19, 19, DISPLAY_BLACK);
          setTextColor(DISPLAY_WHITE);
        } else {
          // Future level items - white background, black text
          fillRect(43 + (i * 12), 20, 19, 19, DISPLAY_WHITE);
          setTextColor(DISPLAY_BLACK);
        }
        
        setCursor(45 + (i * 12), 35);
        print(c);
      }
    }
  } else {
    // Display regular program name (limited to first 8 characters)
    setCursor(6, 35);
    print("Task: ");
    fillRect(50, 20, 110, 20, DISPLAY_WHITE); // Clear area for task name
    String shortProgram = program;
    if (shortProgram.length() > 8) {
      shortProgram = shortProgram.substring(0, 8);
    }
    print(shortProgram);
  }
}

void FED4::displayMouseId() {
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  
  if (!sdCardAvailable) {
    // Show SD card error instead of MouseID
    setCursor(6, 54);
    fillRect(6, 41, 120, 16, DISPLAY_WHITE); // Clear area for mouse ID and label
    print("SD Card error!");
  } else {
    // Show normal MouseID
    setCursor(6, 54);
    print("MouseID: ");
    char idStr[6];  
    int mouseIdNum = mouseId.toInt();
    if (mouseIdNum == 0 && mouseId[0] != '0') {
      // Handle invalid conversion - just display the original string truncated to 4 chars
      snprintf(idStr, sizeof(idStr), "%.4s", mouseId.c_str());
    } else {
      snprintf(idStr, sizeof(idStr), "%04d", mouseIdNum % 10000);
    }
    fillRect(82, 41, 80, 16, DISPLAY_WHITE); // Clear area for mouse ID
    print(idStr);
  }
}

void FED4::displaySex(){
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 71);
  print("Sex: ");
  fillRect(48, 58, 110, 16, DISPLAY_WHITE); // Clear area for sex name
  print(sex);
}

void FED4::displayStrain(){
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 89);
  print("Strain: ");
  fillRect(60, 76, 160, 16, DISPLAY_WHITE); // Clear area for strain name
  print(strain);
}

void FED4::displayAge(){
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 107);
  print("Age:");
  fillRect(42, 94, 160, 16, DISPLAY_WHITE); // Clear area for age name
  print(age);
  print(" months");
}

void FED4::displayEnvironmental(){
  fillRect(0, 0, 176, 17, DISPLAY_BLACK);
  
  setFont(&Org_01);
  setTextSize(2);
  setTextColor(DISPLAY_WHITE);

  setCursor(5, 9);
  print((int)temperature); 
  drawCircle(30, 3, 2, DISPLAY_WHITE); 
  drawCircle(31, 3, 2, DISPLAY_WHITE); 
  setCursor(35, 9);
  print("C");
  
  // Add speaker muted icon if audio is silenced
  if (audioSilenced) {
    setCursor(55, 9);
    print("X"); // X means no audio
  }
}

void FED4::displayBattery(){
  //battery graphic
  fillRect (80, 1, 18, 10, DISPLAY_WHITE); //body
  fillRect (82, 3, 14, 6, DISPLAY_BLACK); //body
  
  fillRect (99, 3, 2, 6, DISPLAY_WHITE);   //terminal

  fillRect (82, 2, (int)((cellVoltage)/7), 8, DISPLAY_WHITE);  //fill

  //battery text
  setFont(&Org_01);
  setTextSize(2);
  setTextColor(DISPLAY_WHITE);
  
  setCursor(105, 9);
  print(cellVoltage, 1);
  print("V");
}

void FED4::displaySDCardStatus() {

}

void FED4::displayCounters()
{
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  
  // Clear all counter value areas with one white rectangle
  fillRect(90, 68, 50, 78, DISPLAY_WHITE);  // Clear area for all counter values
  
  setCursor(30, 80);
  print("Left: ");
  setCursor(90, 80);
  print(leftCount);
  setCursor(30, 100);
  print("Center: ");
  setCursor(90, 100);
  print(centerCount);
  setCursor(30, 120);
  print("Right:  ");
  setCursor(90, 120);
  print(rightCount);
  setCursor(30, 140);
  print("Pellets:");
  setCursor(90, 140);
  print(pelletCount);
}

void FED4::displayIndicators(){
  //TODO: add indicators for when touch flags are set

  //Left 
  fillCircle(17, 75, 5, DISPLAY_WHITE); 
  drawCircle(17, 75, 5, DISPLAY_BLACK);
  if (leftTouch) {
    fillCircle(17, 75, 5, DISPLAY_BLACK); 
  }

  //Center
  fillCircle(17, 95, 5, DISPLAY_WHITE); 
  drawCircle(17, 95, 5, DISPLAY_BLACK);
  if (centerTouch) {
    fillCircle(17, 95, 5, DISPLAY_BLACK); 
  }

  //Right
  fillCircle(17, 115, 5, DISPLAY_WHITE);
  drawCircle(17, 115, 5, DISPLAY_BLACK);
  if (rightTouch) { 
    fillCircle(17, 115, 5, DISPLAY_BLACK); 
  }

  //Pellets 
  fillCircle(17, 135, 5, DISPLAY_WHITE);
  drawCircle(17, 135, 5, DISPLAY_BLACK);
  if (pelletPresent) {
    fillCircle(17, 135, 5, DISPLAY_BLACK); 
  }
}

void FED4::displayDateTime() {
  setFont(&Org_01);
  setTextSize(2);
  setTextColor(DISPLAY_WHITE);

  // Bottom bar anchored to the base of the 320-pixel tall logical display
  fillRect(0, 296, 176, 24, DISPLAY_BLACK);
  DateTime current = rtc.now();

  char timeStr[6];  // HH:MM\0
  char dateStr[9];  // MM.DD.YY\0

  snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d",
           current.month(), current.day(), current.year() - 2000);

  snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
           current.hour(), current.minute());

  setCursor(5, 312);
  print(dateStr);

  setCursor(100, 312);
  print(timeStr);
}

// Displays a low battery warning with an icon and message
void FED4::displayLowBatteryWarning() {
    clearDisplay();
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_BLACK);
    
    // Draw battery outline (x=40, y=40, width=60, height=30)
    int bx = 40, by = 40, bw = 60, bh = 30;
    drawRect(bx, by, bw, bh, DISPLAY_BLACK); // Battery body
    fillRect(bx + bw, by + 8, 6, 14, DISPLAY_BLACK); // Battery terminal
    
    // Draw empty battery (just outline, no fill)
    // Draw exclamation mark inside battery
    int ex = bx + bw/2 - 2;
    int ey = by + 6;
    fillRect(ex, ey, 4, 12, DISPLAY_BLACK); // vertical bar
    fillRect(ex, ey + 16, 4, 4, DISPLAY_BLACK); // dot
    
    // Draw warning text
    setCursor(10, by + bh + 30);
    print("LOW BATTERY");
    setCursor(12, by + bh + 50);
    print("Please charge!");
    refresh();
}

// Resets the display panel via MCP expander RST line.
// Per section 8 of the TN0216 datasheet:
//   RST = HIGH → display OFF (panel blanked, pixel memory retained)
//   RST = LOW  → display ON (normal operation)
// VCOM must be LOW whenever RST is HIGH to prevent shoot-through current.
void FED4::displayReset()
{
    mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);

    // Ensure VCOM is LOW before asserting RST HIGH (shoot-through prevention, section 6-2)
    pinMode(DISPLAY_VCOM, OUTPUT);
    digitalWrite(DISPLAY_VCOM, LOW);
    vcom = false;

    // Brief RST HIGH: blanks the panel so random power-on pixel memory isn't visible
    mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
    delay(10);

    // RST LOW: display enters normal operation — hold LOW for the device lifetime
    mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
    delay(10);
}

// Controls the display frontlight LED via MCP expander
void FED4::displayLight(bool on)
{
    mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
    mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

bool FED4::initializeDisplay()
{
    SPI.setBitOrder(LSBFIRST);

    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(DISPLAY_CS, LOW); // SCS inactive = LOW

    // Allocate framebuffer: 320×176 = 7040 bytes
    if (displayBuffer) {
        free(displayBuffer);
        displayBuffer = nullptr;
    }
    const uint32_t bufferSize = (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8; // 7040 bytes
    displayBuffer = (uint8_t *)malloc(bufferSize);
    if (!displayBuffer) {
        Serial.println("Failed to allocate display buffer");
        return false;
    }
    // All-white initial frame (Data 1 = WHITE, section 9-1); clears random pixel memory at power-on
    memset(displayBuffer, 0xFF, bufferSize);

    // Portrait orientation: logical 176 wide × 320 tall
    // If the display appears rotated, try setRotation(3) as an alternative
    setRotation(1);
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_BLACK);
    setTextWrap(false);

    refresh(); // Push initial white frame to panel
    return true;
}

// Clears the display to white.
// TN0216 has no hardware clear command — write all-white pixels and refresh.
void FED4::clearDisplay()
{
    memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
    refresh();
}

// Sends the full framebuffer to the panel using Kyocera line-oriented SPI protocol (section 9-2).
// Each gate line: 1 address byte + 40 data bytes + 4 dummy bytes = 360 clocks.
// All 176 lines are sent within a single SCS=HIGH window (continuous mode).
void FED4::refresh()
{
    SPI.setBitOrder(LSBFIRST);

    // Toggle VCOM each refresh — AC drive requirement (section 9-3, ≥ ~1 Hz)
    vcom = !vcom;
    digitalWrite(DISPLAY_VCOM, vcom ? HIGH : LOW);

    // tsSCS: SCS must be LOW for ≥ 4 ms before asserting HIGH for the next frame (section 9-4)
    delay(4);

    // Assert SCS active HIGH — all 176 lines sent in one SCS window (section 9-2)
    digitalWrite(DISPLAY_CS, HIGH);

    const uint8_t bytesPerLine = DISPLAY_WIDTH / 8; // 40 bytes = 320 pixels

    for (uint8_t line = 1; line <= DISPLAY_HEIGHT; line++) {
        // Gate address byte (AG0~AG7).
        // LSBFIRST: AG0 (bit 0) is sent first, matching the AG0~AG7 transmission order.
        // Gate line addressing assumed linear from section 7: address = line number (1–176).
        SPI.transfer(line);

        // Pixel data: 40 bytes, D0 (bit 0 of first byte) = leftmost pixel (section 9-1)
        const uint8_t *row = displayBuffer + (uint16_t)(line - 1) * bytesPerLine;
        for (uint8_t b = 0; b < bytesPerLine; b++) {
            SPI.transfer(row[b]);
        }

        // 32 dummy bits (DUM0–DUM31): required for internal panel line processing (section 9-1)
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
    }

    // Deassert SCS; tsSCS and thSCS satisfied by the delay(4) at the start of the next call
    digitalWrite(DISPLAY_CS, LOW);
}

void FED4::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    // Bounds check against logical (rotation-adjusted) dimensions
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
        return;

    // Convert logical → physical coordinates for the current rotation
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
    // case 0: no transform
    }

    if (color)
        displayBuffer[(y * DISPLAY_WIDTH + x) / 8] |= pgm_read_byte(&set[x & 7]);
    else
        displayBuffer[(y * DISPLAY_WIDTH + x) / 8] &= pgm_read_byte(&clr[x & 7]);
}

void FED4::startupAnimation(){
  setTextSize(5);
  setFont(&Org_01);
  setTextColor(DISPLAY_BLACK);

  const char* text = "FED4";  // Text to animate
  int textWidth = 28;         // Approximate width of each character in pixels
  int textX = 176;   // Start position off the screen (right side, logical width)
  int mouseX = 0;
  int centerX = (176 - strlen(text) * textWidth) / 2; // Center X position
  int textY = 60;        // Vertical height of the text

  while (textX > centerX) {
    // Clear only the buffer (not hardware) to prevent flickering
    // Avoid clearDisplay() (which calls refresh) mid-animation — clear the buffer directly
    memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

    //draw FED4
    fillRect(100, 92, 32, 20, DISPLAY_BLACK);    //FED4
    fillRect(112, 82, 16, 8, DISPLAY_BLACK);     //hopper
    fillCircle(108, 98, 3, DISPLAY_WHITE);       //poke 1
    fillCircle(124, 98, 3, DISPLAY_WHITE);       //poke 2
    fillCircle(116, 104, 2, DISPLAY_WHITE);       //poke 3

    // Draw the text sliding in from the right (single render to reduce flickering)
    setCursor(textX, textY);
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
    delay(10);   // Increased delay to reduce flickering (was 1ms, now 10ms)
  }

  // Display the text in the center and hold
  setCursor(textX, textY);
  print(text);
  refresh();
  setTextSize(1);
}

void FED4::displayAudio() {
  setCursor(6, 125);
  print("Audio: ");
  print(audioSilenced ? "Off" : "On");
}

// Display initialization status message below startup animation
void FED4::displayInitStatus(const char* message) {
  setFont(&FreeSans9pt7b);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  
  fillRect(0, 125, 176, 171, DISPLAY_WHITE); // Clear message area (to y=296 bottom bar)
  
  // Display the initialization message
  setCursor(6, 135);
  print("Initializing: ");
  setCursor(6, 158);
  print(message);
  
  refresh();
}