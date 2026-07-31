/*
 * FED4 Display Test
 *
 * Uses the FED4 library display path for the Kyocera 320x176 MIP panel.
 * This matches the current architecture:
 * - SPI display interface
 * - Display reset/backlight controlled via MCP23017 expander
 * - Power rails initialized by FED4::begin()
 */

#include <FED4.h>

FED4 fed4;
unsigned long lastToggleMs = 0;
bool invert = false;

void drawTestScreen() {
  fed4.clearDisplay(); // white background

  fed4.setTextSize(1);
  fed4.setTextColor(DISPLAY_BLACK);
  fed4.setCursor(8, 30);
  fed4.print("FED4 DISPLAY TEST");

  fed4.setCursor(8, 60);
  fed4.print("Panel: TN0216 MIP");

  fed4.setCursor(8, 90);
  fed4.print("SPI + MCP control");

  fed4.setCursor(8, 120);
  fed4.print("Backlight: ");
  fed4.print(invert ? "OFF" : "ON");

  // High-contrast box to validate rendering.
  if (!invert) {
    fed4.fillRect(8, 145, 120, 20, DISPLAY_BLACK);
    fed4.setCursor(12, 160);
    fed4.setTextColor(DISPLAY_WHITE);
    fed4.print("BLACK BAR TEST");
  }

  fed4.refresh();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!fed4.begin("DisplayTest")) {
    Serial.println("FED4 begin() failed");
    while (1) delay(10);
  }

  fed4.displayLight(true);
  drawTestScreen();
}

void loop() {
  // Toggle backlight every 2 seconds to verify MCP display LED control.
  if (millis() - lastToggleMs >= 2000) {
    lastToggleMs = millis();
    invert = !invert;
    fed4.displayLight(!invert);
    drawTestScreen();
  }
}
