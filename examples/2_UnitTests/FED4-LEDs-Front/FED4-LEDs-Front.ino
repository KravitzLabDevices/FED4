/*
 * FED4 Front LEDs Test
 *
 * - Front RGB strip data pin: GPIO 36
 * - Strip power rail: PSV3 via MCP23017 pin 12 (EXP_PSV3_EN, ~ON active-low)
 * - Buttons: 15 / 16 / 39 (INPUT_PULLDOWN)
 * - Main I2C bus: SDA=8, SCL=9
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

#define NUMPIXELS 8
#define BRIGHTNESS 50

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel pixels(NUMPIXELS, RGB_STRIP, NEO_GRB + NEO_KHZ800);

void colorWipe(uint32_t color, unsigned long waitMs) {
  for (int i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, color);
    pixels.show();
    delay(waitMs);
  }
}

void theaterChase(uint32_t color, unsigned long waitMs, unsigned int groupSize, unsigned int numChases) {
  for (unsigned int chase = 0; chase < numChases; chase++) {
    for (unsigned int pos = 0; pos < groupSize; pos++) {
      pixels.clear();
      for (int i = pos; i < pixels.numPixels(); i += groupSize) {
        pixels.setPixelColor(i, color);
      }
      pixels.show();
      delay(waitMs);
    }
  }
}

void rainbow(unsigned long waitMs, unsigned int numLoops) {
  for (unsigned int loop = 0; loop < numLoops; loop++) {
    for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
      for (int i = 0; i < pixels.numPixels(); i++) {
        long pixelHue = firstPixelHue + (i * 65536L / pixels.numPixels());
        pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
      }
      pixels.show();
      delay(waitMs);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed.");
    while (1) delay(10);
  }

  // Enable PSV3 rail (powers front RGB strip)
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV3_EN, LOW); // ~ON active-low
  delay(5);

  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);

  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.clear();
  pixels.show();

  Serial.println("FED4 Front LEDs test ready.");
  Serial.println("Button 1: Rainbow");
  Serial.println("Button 2: Theater Chase");
  Serial.println("Button 3: Color Wipe");
}

void loop() {
  bool b1 = digitalRead(BUTTON_1);
  bool b2 = digitalRead(BUTTON_2);
  bool b3 = digitalRead(BUTTON_3);

  if (b1) {
    Serial.println("Button 1 - Rainbow");
    rainbow(10, 1);
  }
  if (b2) {
    Serial.println("Button 2 - Theater Chase");
    theaterChase(pixels.Color(0, 255, 255), 100, 3, 5);   // cyan
    theaterChase(pixels.Color(255, 0, 255), 100, 3, 5);   // magenta
    theaterChase(pixels.Color(255, 255, 0), 100, 3, 5);   // yellow
  }
  if (b3) {
    Serial.println("Button 3 - Color Wipe");
    colorWipe(pixels.Color(255, 0, 0), 25);               // red
    colorWipe(pixels.Color(0, 255, 0), 25);               // green
    colorWipe(pixels.Color(0, 0, 255), 25);               // blue
    colorWipe(pixels.Color(255, 255, 255), 25);           // white
  }

  // Clear when idle
  if (!b1 && !b2 && !b3) {
    pixels.clear();
    pixels.show();
  }

  delay(10);
}
