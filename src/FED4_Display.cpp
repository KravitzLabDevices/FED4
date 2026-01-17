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

    // draw line to split on screen text 
    drawLine(0,59,168,59, DISPLAY_BLACK);  
    drawLine(0,60,168,60, DISPLAY_BLACK);  

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

  // draw line to split on screen text 
  drawLine(0,59,168,59, DISPLAY_BLACK);  
  drawLine(0,60,168,60, DISPLAY_BLACK);  

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
    setCursor(6, 53);
    fillRect(6, 40, 120, 16, DISPLAY_WHITE); // Clear area for mouse ID and label
    print("SD Card error!");
  } else {
    // Show normal MouseID
    setCursor(6, 53);
    print("MouseID: ");
    char idStr[6];  
    int mouseIdNum = mouseId.toInt();
    if (mouseIdNum == 0 && mouseId[0] != '0') {
      // Handle invalid conversion - just display the original string truncated to 4 chars
      snprintf(idStr, sizeof(idStr), "%.4s", mouseId.c_str());
    } else {
      snprintf(idStr, sizeof(idStr), "%04d", mouseIdNum % 10000);
    }
    fillRect(82, 40, 80, 16, DISPLAY_WHITE); // Clear area for mouse ID
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
  //try to make text inverse white on black
  fillRect (0, 0, 144, 17, DISPLAY_BLACK);
  
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

  // Print date and time at bottom of the screen
  fillRect(0, 146, 144, 22, DISPLAY_BLACK);
  DateTime current = rtc.now();
  
  // Buffer for formatting time
  char timeStr[6];  // HH:MM\0
  char dateStr[9];  // MM.DD.YY\0
  
  // Format date string
  snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d", 
           current.month(), 
           current.day(), 
           current.year() - 2000);
           
  // Format time string
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", 
           current.hour(), 
           current.minute());

  // Display formatted strings
  setCursor(5, 160);
  print(dateStr);
  
  setCursor(94, 160);
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

/**
 * Initializes the Sharp Memory Display
 * 
 * @return bool - true if initialization successful, false if buffer allocation fails
 * 
 * This function:
 * - Sets up the display CS pin and SPI bit order
 * - Allocates display buffer memory
 * - Sends initial clear command to display
 * - Configures default display settings (rotation, font, text properties)
 * - Performs initial refresh
 */
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

    delay(1); // Give display time to process clear command

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

    // Use static buffer to avoid stack allocation on every call
    static uint8_t lineBuffer[22]; // Maximum size needed: bytes_per_line + 2 = 18 + 2 = 20, rounded up to 22

    // Send all lines
    for (i = 0; i < totalbytes; i += bytes_per_line)
    {
        // Send address byte (line number)
        currentline = ((i + 1) / (DISPLAY_WIDTH / 8)) + 1;
        lineBuffer[0] = currentline;

        // Copy display data for this line
        memcpy(lineBuffer + 1, displayBuffer + i, bytes_per_line);

        // Add end of line marker
        lineBuffer[bytes_per_line + 1] = 0x00;

        // Send the entire line at once
        for (uint8_t j = 0; j < bytes_per_line + 2; j++)
        {
            SPI.transfer(lineBuffer[j]);
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
  setTextColor(DISPLAY_BLACK);

  const char* text = "FED4";  // Text to animate
  int textWidth = 28;         // Approximate width of each character in pixels
  int textX = 144;   // Start position off the screen (right side)
  int mouseX = 0;
  int centerX = (144 - strlen(text) * textWidth) / 2; // Center X position
  int textY = 60;        // Vertical height of the text

  while (textX > centerX) {
    clearDisplay();

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
  delay(1500); // Pause to keep "FED4" displayed for 1.5s
}

void FED4::displayAudio() {
  setCursor(6, 125);
  print("Audio: ");
  print(audioSilenced ? "Off" : "On");
}