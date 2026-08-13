#include "FED4.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

// Display: Kyocera TN0216ANVNANN-GN00  320×176 Memory-in-Pixel (MIP)
// Interface: 3-wire SPI (SCLK, SCS, SI) + RST + VCOM
// RST = LOW → display ON;  RST = HIGH → display OFF (VCOM must be LOW when RST HIGH)
// Pixel data: 0 = BLACK, 1 = WHITE  (section 9-1)
// SPI bit order: LSBFIRST — AG0 / D0 are the "first" bits per the datasheet notation,
//   meaning they map to bit 0 (transmitted first by LSBFIRST).
// Gate address: linear 0–175 (Demo-Hardware / SleepModes). Not 1–176.
//
// VCOM policy (Demo-Hardware / FED4-SleepModes):
//   - refresh(): GPIO invert only (phase-locked to SCS), then leave pin static.
//   - light sleep: GPIO toggle every 500 ms between esp_light_sleep chunks (see FED4_Sleep.cpp).
// LEDC KEEP_ALIVE helpers remain for RST safety / optional experiments — not used in sleep path.
// Do not use analogWrite() on other channels that share LEDC timers while VCOM LEDC is active.

static const ledc_timer_t VCOM_LEDC_TIMER = LEDC_TIMER_1;
static const ledc_channel_t VCOM_LEDC_CHANNEL = LEDC_CHANNEL_1;
static const ledc_mode_t VCOM_LEDC_MODE = LEDC_LOW_SPEED_MODE;
static const ledc_timer_bit_t VCOM_LEDC_RES = LEDC_TIMER_14_BIT;
static const uint32_t VCOM_LEDC_HZ = 30;
static const uint32_t VCOM_LEDC_DUTY_50 = 8192; // 50% of 2^14

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

// Demo-Hardware header metrics (default GFX font: cursor Y = top edge of glyph)
static const int16_t HEADER_H = 20;
static const int16_t HEADER_TEXT_Y = 5;
static const int16_t CONTENT_TOP = 28;
static const int16_t DIVIDER_Y = 70;
static const int16_t COUNTERS_TOP = 88;

void FED4::updateDisplay() {
  // Demo ground truth: default GFX font for body; FreeSans reserved for labels
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  // Full clear each status frame — MIP retains uncleared pixels otherwise
  memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

  displayTask();
  displayMouseId();

  drawLine(0, DIVIDER_Y, 175, DIVIDER_Y, DISPLAY_BLACK);

  displayEnvironmental();
  displayBattery();
  displaySDCardStatus();
  displayCounters();
  displayIndicators();
  displayDateTime();

  refresh();
}

void FED4::displayActivityMonitor() {
  // Use the same layout as normal FED4 display but replace counters and indicators
  displayTask();
  displayMouseId();

  drawLine(0, DIVIDER_Y, 175, DIVIDER_Y, DISPLAY_BLACK);

  // draw screen elements (same as normal display)
  displayEnvironmental();
  displayBattery();
  displaySDCardStatus();

  // Replace displayCounters() with activity information
  displayActivityCounters();

  displayDateTime();
}

void FED4::displayActivityCounters() {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  fillRect(90, COUNTERS_TOP - 4, 70, 78, DISPLAY_WHITE);

  setCursor(6, COUNTERS_TOP);
  print("Activity ");
  setCursor(90, COUNTERS_TOP);
  print(motionCount);

  setCursor(6, COUNTERS_TOP + 20);
  print("Activity% ");
  setCursor(90, COUNTERS_TOP + 20);

  printf("%.1f", motionPercentage);

  setCursor(6, COUNTERS_TOP + 40);
  print("Seconds");
  setCursor(90, COUNTERS_TOP + 40);

  if (pollSensorsTimer == 0) {
    pollSensorsTimer = millis();
  }

  print((millis() - pollSensorsTimer) / 1000);

  setCursor(6, COUNTERS_TOP + 60);
  print("Uptime(h)");
  setCursor(90, COUNTERS_TOP + 60);
  float uptimeHours = millis() / 1000.0 / 3600.0;
  printf("%.2f", uptimeHours);
}

void FED4::displayTask() {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  if (program == "SequenceLearning") {
    setCursor(6, CONTENT_TOP + 8);
    print("Seq:");

    fillRect(40, CONTENT_TOP, 120, 30, DISPLAY_WHITE);

    if (currentSequence.length() > 0) {
      setCursor(50, CONTENT_TOP + 8);

      for (int i = 0; i < currentSequence.length(); i++) {
        char c = currentSequence[i];

        if (i < currentSequenceLevel) {
          fillRect(43 + (i * 12), CONTENT_TOP, 19, 19, DISPLAY_BLACK);
          setTextColor(DISPLAY_WHITE);
        } else {
          fillRect(43 + (i * 12), CONTENT_TOP, 19, 19, DISPLAY_WHITE);
          setTextColor(DISPLAY_BLACK);
        }

        setCursor(45 + (i * 12), CONTENT_TOP + 8);
        print(c);
      }
    }
  } else {
    setCursor(6, CONTENT_TOP + 8);
    print("Task: ");
    fillRect(70, CONTENT_TOP, 100, 16, DISPLAY_WHITE);
    String shortProgram = program;
    if (shortProgram.length() > 8) {
      shortProgram = shortProgram.substring(0, 8);
    }
    print(shortProgram);
  }
}

void FED4::displayMouseId() {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  const int16_t mouseY = CONTENT_TOP + 28; // room below Task; gap above divider

  if (!sdCardAvailable) {
    setCursor(6, mouseY);
    fillRect(6, mouseY - 8, 160, 14, DISPLAY_WHITE);
    print("No SD — not logging");
  } else {
    setCursor(6, mouseY);
    print("MouseID: ");
    char idStr[6];
    int mouseIdNum = mouseId.toInt();
    if (mouseIdNum == 0 && mouseId[0] != '0') {
      snprintf(idStr, sizeof(idStr), "%.4s", mouseId.c_str());
    } else {
      snprintf(idStr, sizeof(idStr), "%04d", mouseIdNum % 10000);
    }
    fillRect(100, mouseY - 8, 70, 14, DISPLAY_WHITE);
    print(idStr);
  }
}

void FED4::displaySex(){
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 82);
  print("Sex: ");
  fillRect(48, 74, 120, 14, DISPLAY_WHITE);
  print(sex);
}

void FED4::displayStrain(){
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 100);
  print("Strain: ");
  fillRect(76, 92, 96, 14, DISPLAY_WHITE);
  print(strain);
}

void FED4::displayAge(){
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 118);
  print("Age:");
  fillRect(48, 110, 120, 14, DISPLAY_WHITE);
  print(age);
  print(" mo");
}

void FED4::displayEnvironmental(){
  // Demo: HEADER_H=20, default-font text at Y=5 (top edge of glyph)
  fillRect(0, 0, 176, HEADER_H, DISPLAY_BLACK);

  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_WHITE);

  setCursor(4, HEADER_TEXT_Y);
  print((int)temperature);
  print("C");

  if (humidity >= 0) {
    setCursor(36, HEADER_TEXT_Y);
    print((int)humidity);
    print("%");
  }

  if (audioSilenced) {
    setCursor(70, HEADER_TEXT_Y);
    print("X");
  }
}

void FED4::displayBattery(){
  // Demo-style header battery: 7px tall, optically centered in HEADER_H
  static const int16_t BAR_H = 7;
  static const int16_t BAR_W = 18;
  static const int16_t INNER_H = 5;
  const int16_t barY = (HEADER_H - BAR_H) / 2; // 6 in a 20px bar
  const int16_t barX = 118;
  const int16_t innerY = barY + (BAR_H - INNER_H) / 2;

  fillRect(barX, barY, BAR_W, BAR_H, DISPLAY_WHITE);
  fillRect(barX + 2, innerY, 14, INNER_H, DISPLAY_BLACK);
  fillRect(barX + BAR_W, innerY, 2, INNER_H, DISPLAY_WHITE); // terminal
  int fillW = (int)(cellVoltage / 7);
  if (fillW < 0) fillW = 0;
  if (fillW > 14) fillW = 14;
  if (fillW > 0) {
    fillRect(barX + 2, innerY, fillW, INNER_H, DISPLAY_WHITE);
  }

  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_WHITE);

  setCursor(142, HEADER_TEXT_Y);
  print(cellVoltage, 1);
  print("V");
}

void FED4::displaySDCardStatus() {

}

void FED4::displayCounters()
{
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  // Row tops shared with displayIndicators() — default font Y is glyph top (~8px tall)
  fillRect(90, COUNTERS_TOP - 2, 70, 78, DISPLAY_WHITE);

  setCursor(30, COUNTERS_TOP);
  print("Left: ");
  setCursor(90, COUNTERS_TOP);
  print(leftCount);
  setCursor(30, COUNTERS_TOP + 20);
  print("Center: ");
  setCursor(90, COUNTERS_TOP + 20);
  print(centerCount);
  setCursor(30, COUNTERS_TOP + 40);
  print("Right: ");
  setCursor(90, COUNTERS_TOP + 40);
  print(rightCount);
  setCursor(30, COUNTERS_TOP + 60);
  print("Pellets:");
  setCursor(90, COUNTERS_TOP + 60);
  print(pelletCount);
}

void FED4::displayIndicators(){
  // Circle center = text top + 3 → optical middle of 8px default font
  static const int16_t ROW = 20;
  static const int16_t DOT_X = 17;
  static const int16_t DOT_R = 4;
  static const int16_t DOT_DY = 3;

  // Live well state — cached pelletPresent can lag after LatePelletTaken / sleep
  pelletPresent = checkForPellet();
  const bool filled[4] = {leftTouch, centerTouch, rightTouch, pelletPresent};

  for (int i = 0; i < 4; i++) {
    const int16_t cy = COUNTERS_TOP + i * ROW + DOT_DY;
    fillCircle(DOT_X, cy, DOT_R, DISPLAY_WHITE);
    drawCircle(DOT_X, cy, DOT_R, DISPLAY_BLACK);
    if (filled[i]) {
      fillCircle(DOT_X, cy, DOT_R, DISPLAY_BLACK);
    }
  }
}

void FED4::displayDateTime() {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_WHITE);

  // Bottom bar — Demo FOOTER_Y=302, text at +5 (default font top-edge)
  static const int16_t FOOTER_Y = 302;
  static const int16_t FOOTER_TEXT_Y = FOOTER_Y + 5;
  fillRect(0, FOOTER_Y, 176, 320 - FOOTER_Y, DISPLAY_BLACK);
  DateTime current = rtc.now();

  char dateStr[9];
  snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d",
           current.month(), current.day(), current.year() - 2000);

  int h24 = current.hour();
  int h12 = h24 % 12;
  if (h12 == 0) {
    h12 = 12;
  }
  char timeStr[10];
  snprintf(timeStr, sizeof(timeStr), "%d:%02d%s", h12, current.minute(),
           (h24 >= 12) ? "PM" : "AM");

  setCursor(5, FOOTER_TEXT_Y);
  print(dateStr);

  setCursor(100, FOOTER_TEXT_Y);
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

// Release LEDC from DISPLAY_VCOM without forcing a polarity (refresh sets the next level).
static void detachVcomLedc(bool &vcomLedcActive)
{
    if (!vcomLedcActive) {
        return;
    }
    ledc_stop(VCOM_LEDC_MODE, VCOM_LEDC_CHANNEL, 0);
    vcomLedcActive = false;
}

void FED4::stopVcomLedc()
{
    detachVcomLedc(vcomLedcActive);
    pinMode(DISPLAY_VCOM, OUTPUT);
    digitalWrite(DISPLAY_VCOM, LOW);
    vcom = false;
}

// End sleep KEEP_ALIVE; leave GPIO at last refresh() polarity (do not force LOW).
void FED4::releaseVcomLedcToGpio()
{
    detachVcomLedc(vcomLedcActive);
    pinMode(DISPLAY_VCOM, OUTPUT);
    digitalWrite(DISPLAY_VCOM, vcom ? HIGH : LOW);
}

bool FED4::startVcomLedc()
{
    // Keep RC_FAST powered in light sleep (LEDC clock for KEEP_ALIVE)
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);

    ledc_timer_config_t timer = {};
    timer.speed_mode = VCOM_LEDC_MODE;
    timer.timer_num = VCOM_LEDC_TIMER;
    timer.duty_resolution = VCOM_LEDC_RES;
    timer.freq_hz = VCOM_LEDC_HZ;
    timer.clk_cfg = (ledc_clk_cfg_t)LEDC_USE_RC_FAST_CLK;

    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        Serial.printf("startVcomLedc: timer config failed: %s\n", esp_err_to_name(err));
        vcomLedcActive = false;
        return false;
    }

    ledc_channel_config_t channel = {};
    channel.gpio_num = DISPLAY_VCOM;
    channel.speed_mode = VCOM_LEDC_MODE;
    channel.channel = VCOM_LEDC_CHANNEL;
    channel.timer_sel = VCOM_LEDC_TIMER;
    channel.duty = VCOM_LEDC_DUTY_50;
    channel.hpoint = 0;
    channel.sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE;

    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        Serial.printf("startVcomLedc: channel config failed: %s\n", esp_err_to_name(err));
        vcomLedcActive = false;
        return false;
    }

    gpio_sleep_sel_dis((gpio_num_t)DISPLAY_VCOM);
    vcomLedcActive = true;
    return true;
}

// Resets the display panel via MCP expander RST line.
// Per section 8 of the TN0216 datasheet:
//   RST = HIGH → display OFF (panel blanked, pixel memory retained)
//   RST = LOW  → display ON (normal operation)
// VCOM must be LOW whenever RST is HIGH to prevent shoot-through current.
void FED4::displayReset()
{
    mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);

    // Stop LEDC and force VCOM LOW before RST HIGH (section 6-2)
    stopVcomLedc();

    // Brief RST HIGH: blanks the panel so random power-on pixel memory isn't visible
    mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
    delay(10);

    // RST LOW: display enters normal operation — hold LOW for the device lifetime
    mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
    delay(10);

    pinMode(DISPLAY_VCOM, OUTPUT);
    digitalWrite(DISPLAY_VCOM, LOW);
    vcom = false;
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

    // Portrait orientation: logical 176 wide × 320 tall (rotation 3 = 180° flip)
    setRotation(DISPLAY_ROTATION);
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_BLACK);
    setTextWrap(false);

    refresh(); // Push initial white frame (GPIO VCOM)

    return true;
}

bool FED4::orientScreen()
{
    float x, y, z;
    readAccel(x, y, z);
    const float xG = x / FED4_GRAVITY_MS2;
    const uint8_t rot = fed4DisplayRotationForAccelX(xG);
    if (rotation == rot)
        return false;

    setRotation(rot);
    return true;
}

// Clears the display to white.
// TN0216 has no hardware clear command — write all-white pixels and refresh.
void FED4::clearDisplay()
{
    if (!displayBuffer) {
        return;
    }
    memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
    refresh();
}

// After SD (often 4 MHz / MSBFIRST), reclaim the shared bus for Kyocera (Demo: 1 MHz LSBFIRST).
void FED4::reclaimSpiForDisplay()
{
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    pinMode(DISPLAY_CS, OUTPUT);
    digitalWrite(DISPLAY_CS, LOW);
    SPI.setFrequency(1000000);
    SPI.setBitOrder(LSBFIRST);
    SPI.setDataMode(SPI_MODE0);
}

// Sends the full framebuffer to the panel using Kyocera line-oriented SPI protocol (section 9-2).
// Each gate line: 1 address byte + 40 data bytes + 4 dummy bytes = 360 clocks.
// All 176 lines are sent within a single SCS=HIGH window (continuous mode).
void FED4::refresh()
{
    if (!displayBuffer) {
        return;
    }

    reclaimSpiForDisplay();

    // Demo-Hardware: phase-lock VCOM to this frame (GPIO only)
    detachVcomLedc(vcomLedcActive); // no-op if LEDC unused
    vcom = !vcom;
    pinMode(DISPLAY_VCOM, OUTPUT);
    digitalWrite(DISPLAY_VCOM, vcom ? HIGH : LOW);

    // tsSCS: SCS must be LOW for ≥ 4 ms before asserting HIGH (section 9-4)
    delay(4);
    delayMicroseconds(30);

    // Assert SCS active HIGH — all 176 lines in one SCS window (section 9-2)
    digitalWrite(DISPLAY_CS, HIGH);
    delay(5); // Demo-Hardware settle after SCS↑

    SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));

    const uint8_t bytesPerLine = DISPLAY_WIDTH / 8; // 40 bytes = 320 pixels

    // Gate addresses 0..175 — match FED4-Demo-Hardware / SleepModes (not 1..176).
    // LSBFIRST: AG0 (bit 0) first. Row i in the framebuffer → address i.
    for (uint8_t line = 0; line < DISPLAY_HEIGHT; line++) {
        SPI.transfer(line);

        // Pixel data: 40 bytes, D0 (bit 0 of first byte) = leftmost pixel (section 9-1)
        const uint8_t *row = displayBuffer + (uint16_t)line * bytesPerLine;
        for (uint8_t b = 0; b < bytesPerLine; b++) {
            SPI.transfer(row[b]);
        }

        // 32 dummy bits (DUM0–DUM31): required for internal panel line processing (section 9-1)
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
    }

    SPI.endTransaction();
    delay(2); // Demo-Hardware: hold before SCS↓
    digitalWrite(DISPLAY_CS, LOW);
}

void FED4::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (!displayBuffer) {
        return;
    }

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
  if (!displayBuffer) {
    return;
  }

  // Demo ground truth: default GFX font for dense UI; Org_01 only for big logo text
  setTextSize(5);
  setFont(&Org_01);
  setTextColor(DISPLAY_BLACK);

  const char* text = "FED4";
  const int textWidth = 28;
  int textX = 176;
  int mouseX = 0;
  const int centerX = (176 - (int)strlen(text) * textWidth) / 2;
  const int textY = 60;
  // ~1s total: fewer frames, full-buffer clear each frame (avoids MIP trails)
  const int stepPx = 16;

  while (textX > centerX) {
    // Full white clear every frame — required on MIP (no auto-erase)
    memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

    fillRect(100, 92, 32, 20, DISPLAY_BLACK);
    fillRect(112, 82, 16, 8, DISPLAY_BLACK);
    fillCircle(108, 98, 3, DISPLAY_WHITE);
    fillCircle(124, 98, 3, DISPLAY_WHITE);
    fillCircle(116, 104, 2, DISPLAY_WHITE);

    setCursor(textX, textY);
    print(text);

    textX -= stepPx;
    if (textX < centerX) {
      textX = centerX;
    }
    mouseX += stepPx / 2;

    fillRoundRect(mouseX + 25, 92, 15, 10, 7, DISPLAY_BLACK);
    fillRoundRect(mouseX + 22, 90, 8, 5, 3, DISPLAY_BLACK);
    fillRoundRect(mouseX + 30, 94, 1, 1, 1, DISPLAY_WHITE);

    if ((mouseX / 10) % 2 == 0) {
      fillRoundRect(mouseX, 94, 32, 17, 10, DISPLAY_BLACK);
      drawFastHLine(mouseX - 8, 95, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX - 8, 96, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX - 14, 94, 8, DISPLAY_BLACK);
      drawFastHLine(mouseX - 14, 95, 8, DISPLAY_BLACK);
      fillRoundRect(mouseX + 22, 109, 8, 4, 3, DISPLAY_BLACK);
      fillRoundRect(mouseX, 107, 8, 6, 3, DISPLAY_BLACK);
    } else {
      fillRoundRect(mouseX + 2, 92, 30, 17, 10, DISPLAY_BLACK);
      drawFastHLine(mouseX - 6, 101, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX - 6, 100, 18, DISPLAY_BLACK);
      drawFastHLine(mouseX - 12, 102, 8, DISPLAY_BLACK);
      drawFastHLine(mouseX - 12, 101, 8, DISPLAY_BLACK);
      fillRoundRect(mouseX + 15, 109, 8, 4, 3, DISPLAY_BLACK);
      fillRoundRect(mouseX + 8, 107, 8, 6, 3, DISPLAY_BLACK);
    }
    refresh();
  }

  // Final centered frame, then hard clear so init UI has no leftover pixels
  memset(displayBuffer, 0xFF, (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
  setCursor(centerX, textY);
  print(text);
  fillRect(100, 92, 32, 20, DISPLAY_BLACK);
  fillRect(112, 82, 16, 8, DISPLAY_BLACK);
  fillCircle(108, 98, 3, DISPLAY_WHITE);
  fillCircle(124, 98, 3, DISPLAY_WHITE);
  fillCircle(116, 104, 2, DISPLAY_WHITE);
  refresh();
  delay(200);

  clearDisplay(); // full white — removes any residual animation pixels
  setFont(nullptr);
  setTextSize(1);
}

void FED4::displayAudio() {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);
  setCursor(6, 136);
  print("Audio: ");
  print(audioSilenced ? "Off" : "On");
}

// Display initialization status message below startup animation
void FED4::displayInitStatus(const char* message) {
  // Called during begin() before the framebuffer exists — Serial-only until then
  if (!displayBuffer) {
    return;
  }

  // Demo body style
  setFont(nullptr);
  setTextSize(1);
  setTextColor(DISPLAY_BLACK);

  fillRect(0, 125, 176, 171, DISPLAY_WHITE);

  setCursor(6, 140);
  print("Initializing:");
  setCursor(6, 156);
  print(message);

  refresh();
}