/*
 * FED4 Display Standalone Test
 *
 * Drives the Kyocera TN0216 MIP panel without FED4.h.
 * Mirrors the SPI + MCP control path in src/FED4_Display.cpp.
 *
 * Hardware:
 *   SPI:  SCK=12, MOSI=13, CS(SCS)=44, VCOM=43
 *   I2C:  SDA=8, SCL=9 (MCP23017)
 *   MCP:  EXP_DISPLAY_RESET=6, EXP_DISPLAY_LED=7
 *         EXP_PSV2_EN=13, EXP_PSV3_EN=12 (enable both rails, ~ON active-low)
 *
 * Panel: 320 x 176 physical pixels, 3-wire SPI, LSBFIRST
 * RGB bit polarity: 0 = BLACK, 1 = WHITE
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

// Physical panel size (Kyocera TN0216) — used by SPI refresh
static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;

static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;

Adafruit_MCP23X17 mcp;

// Physical framebuffer is 320x176; setRotation(1) → logical 176x320
class MIPDisplay : public Adafruit_GFX {
public:
  MIPDisplay() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

  bool begin() {
    const uint32_t bufferSize = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8; // 7040
    if (frameBuffer) {
      free(frameBuffer);
      frameBuffer = nullptr;
    }
    frameBuffer = (uint8_t *)malloc(bufferSize);
    if (!frameBuffer) {
      Serial.println("Failed to allocate framebuffer");
      return false;
    }
    memset(frameBuffer, 0xFF, bufferSize); // all white
    setRotation(1);
    return true;
  }

  void clearWhite() {
    memset(frameBuffer, 0xFF, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void refresh() {
    SPI.setBitOrder(LSBFIRST);

    // Toggle VCOM each refresh (AC drive)
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);

    // SCS must be LOW ≥ 4 ms before next frame
    delay(4);

    digitalWrite(DISPLAY_CS, HIGH);

    const uint8_t bytesPerLine = PANEL_WIDTH / 8; // 40
    for (uint8_t line = 1; line <= PANEL_HEIGHT; line++) {
      SPI.transfer(line); // gate address
      const uint8_t *row = frameBuffer + (uint16_t)(line - 1) * bytesPerLine;
      for (uint8_t b = 0; b < bytesPerLine; b++) {
        SPI.transfer(row[b]);
      }
      // 32 dummy clocks
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
    }

    digitalWrite(DISPLAY_CS, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
      return;

    int16_t px = x;
    int16_t py = y;

    // Convert logical → physical for current rotation
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

    const uint32_t index = ((uint32_t)py * PANEL_WIDTH + (uint32_t)px) / 8;
    const uint8_t bit = px & 7;
    if (color)
      frameBuffer[index] |= (1 << bit);
    else
      frameBuffer[index] &= ~(1 << bit);
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

  // VCOM must be LOW whenever RST is HIGH (shoot-through prevention)
  pinMode(DISPLAY_VCOM, OUTPUT);
  digitalWrite(DISPLAY_VCOM, LOW);

  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW); // RST LOW = display ON
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

void drawTestScreen() {
  display.clearWhite();

  display.setTextSize(1);
  display.setTextColor(PIXEL_BLACK);
  display.setCursor(8, 30);
  display.print("FED4 DISPLAY STANDALONE");

  display.setCursor(8, 60);
  display.print("Panel: TN0216 MIP");

  display.setCursor(8, 90);
  display.print("No FED4.h — SPI+MCP");

  display.setCursor(8, 120);
  display.print("Backlight: ");
  display.print(backlightOn ? "ON" : "OFF");

  display.fillRect(8, 145, 140, 24, PIXEL_BLACK);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(12, 162);
  display.print("BLACK BAR TEST");

  // Border for geometry check
  display.drawRect(0, 0, display.width() - 1, display.height() - 1, PIXEL_BLACK);

  display.refresh();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Display Standalone Test ===");

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1) delay(10);
  }

  // Enable power rails
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW); // ~ON active-low
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delay(5);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);
  SPI.setBitOrder(LSBFIRST);

  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW); // SCS inactive = LOW

  displayReset();
  displayLight(true);

  if (!display.begin()) {
    Serial.println("Display begin failed");
    while (1) delay(10);
  }

  drawTestScreen();
  Serial.println("Display initialized.");
}

void loop() {
  if (millis() - lastToggleMs >= 2000) {
    lastToggleMs = millis();
    backlightOn = !backlightOn;
    displayLight(backlightOn);
    drawTestScreen();
  }
}
