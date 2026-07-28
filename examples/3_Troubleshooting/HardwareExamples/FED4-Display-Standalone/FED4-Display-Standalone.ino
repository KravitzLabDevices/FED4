/*
 * FED4 Display Standalone Test
 *
 * Kyocera TN0216ANVNANN-GN00 (320×176 MIP, 3-wire SPI) without FED4.h.
 *
 * Hardware:
 *   SPI:  SCK=12, MOSI=13, CS(SCS)=44, VCOM=43
 *   I2C:  SDA=8, SCL=9 (MCP23017)
 *   MCP:  EXP_DISPLAY_RESET=6, EXP_DISPLAY_LED=7
 *         EXP_PSV2_EN=13, EXP_PSV3_EN=12 (~ON active-low)
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display pins).
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <Fonts/FreeSans9pt7b.h>
#include <FED4_Pins.h>

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;
static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;
static const uint32_t SPI_HZ = 1000000;

// LSBFIRST: bit 0 = leftmost pixel in each byte group
static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

Adafruit_MCP23X17 mcp;

class MIPDisplay : public Adafruit_GFX {
public:
  MIPDisplay() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

  bool begin() {
    const uint32_t bufferSize = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8;
    if (frameBuffer) {
      free(frameBuffer);
      frameBuffer = nullptr;
    }
    frameBuffer = (uint8_t *)malloc(bufferSize);
    if (!frameBuffer) {
      Serial.println("Failed to allocate framebuffer");
      return false;
    }
    memset(frameBuffer, 0x00, bufferSize); // dark theme default
    setRotation(3); // match FED4.h DISPLAY_ROTATION (180° from rotation 1)
    return true;
  }

  void clearBlack() {
    memset(frameBuffer, 0x00, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void clearWhite() {
    memset(frameBuffer, 0xFF, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void refresh() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
    delay(4);

    delayMicroseconds(30);
    digitalWrite(DISPLAY_CS, HIGH);
    delay(5); // tsSCS: SCS high ≥ 4 ms before first clock

    SPI.beginTransaction(SPISettings(SPI_HZ, LSBFIRST, SPI_MODE0));

    const uint8_t bytesPerLine = PANEL_WIDTH / 8;
    for (uint8_t line = 0; line < PANEL_HEIGHT; line++) {
      SPI.transfer(line);
      const uint8_t *row = frameBuffer + (uint32_t)line * bytesPerLine;
      for (uint8_t b = 0; b < bytesPerLine; b++) {
        SPI.transfer(row[b]);
      }
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
    }

    SPI.endTransaction();
    delay(2); // thSCS: hold SCS high ≥ 1 ms after last clock
    digitalWrite(DISPLAY_CS, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
      return;

    int16_t px = x;
    int16_t py = y;

    switch (rotation) {
      case 1:
        _swap_int16_t(px, py);
        px = PANEL_WIDTH - 1 - px;
        break;
      case 2:
        px = PANEL_WIDTH - 1 - px;
        py = PANEL_HEIGHT - 1 - py;
        break;
      case 3:
        _swap_int16_t(px, py);
        py = PANEL_HEIGHT - 1 - py;
        break;
      default:
        break;
    }

    if (color)
      frameBuffer[(py * PANEL_WIDTH + px) / 8] |= pgm_read_byte(&set[px & 7]);
    else
      frameBuffer[(py * PANEL_WIDTH + px) / 8] &= pgm_read_byte(&clr[px & 7]);
  }

private:
  uint8_t *frameBuffer = nullptr;
  bool vcomState = false;
};

MIPDisplay display;
unsigned long lastToggleMs = 0;
bool backlightOn = true;

void displayReset() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  pinMode(DISPLAY_VCOM, OUTPUT);
  digitalWrite(DISPLAY_VCOM, LOW);

  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

void drawTestScreen() {
  display.clearBlack();

  display.drawRect(0, 0, display.width() - 1, display.height() - 1, PIXEL_WHITE);

  // Proportional font title
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(6, 28);
  display.print("FED4 Display");

  // Built-in font at three scales
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setCursor(8, 52);
  display.print("Size 1");

  display.setTextSize(2);
  display.setCursor(8, 72);
  display.print("Size 2");

  display.setTextSize(3);
  display.setCursor(8, 104);
  display.print("Sz3");

  display.setTextSize(1);
  display.setCursor(8, 148);
  display.print("Panel: TN0216 MIP");

  display.setCursor(8, 164);
  display.print("BL: ");
  display.print(backlightOn ? "ON" : "OFF");

  // Shapes (white on black)
  display.drawLine(8, 182, 168, 182, PIXEL_WHITE);
  display.fillRect(8, 192, 56, 36, PIXEL_WHITE);
  display.drawRect(72, 192, 48, 36, PIXEL_WHITE);
  display.fillCircle(148, 210, 16, PIXEL_WHITE);
  display.drawTriangle(8, 268, 36, 240, 64, 268, PIXEL_WHITE);

  // Inverted label inside white box
  display.setTextColor(PIXEL_BLACK);
  display.setCursor(18, 214);
  display.print("BOX");

  display.setTextColor(PIXEL_WHITE);
  display.setCursor(80, 214);
  display.print("rect");

  // Bottom bar (light on dark theme)
  display.fillRect(0, 296, display.width(), 24, PIXEL_WHITE);
  display.setTextColor(PIXEL_BLACK);
  display.setCursor(8, 312);
  display.print("dark theme + fonts");

  display.refresh();
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== FED4 Display Standalone ===");

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW);
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delay(5);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  displayReset();
  displayLight(true);

  if (!display.begin()) {
    Serial.println("Display begin failed");
    while (1) delay(10);
  }

  display.clearBlack();
  display.refresh();

  drawTestScreen();
  Serial.println("Display ready.");
}

void loop() {
  if (millis() - lastToggleMs >= 2000) {
    lastToggleMs = millis();
    backlightOn = !backlightOn;
    displayLight(backlightOn);
    drawTestScreen();
  }
}
