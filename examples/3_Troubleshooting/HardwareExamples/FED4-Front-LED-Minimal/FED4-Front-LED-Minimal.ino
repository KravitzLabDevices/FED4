/*
  FED4 Front LED Minimal Test
  - Bypasses FED4 library
  - Enables LDO3 via MCP23017
  - Drives WS2812B strip on pin 36 with Adafruit_NeoPixel
*/

#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>

// Front LED strip
#define RGB_STRIP_PIN 36
#define NUMPIXELS 8
#define BRIGHTNESS 80

// Power rails / MCP
#define LDO2_ENABLE 47
#define MCP_ADDR 0x20
#define LDO3_PIN 14

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel strip(NUMPIXELS, RGB_STRIP_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(50);

  // Enable LDO2 (powers MCP + I2C rails)
  pinMode(LDO2_ENABLE, OUTPUT);
  digitalWrite(LDO2_ENABLE, HIGH);
  delay(5);

  if (!mcp.begin_I2C(MCP_ADDR)) {
    Serial.println("MCP23017 not found");
    while (true) { delay(1000); }
  }

  // Enable LDO3 (powers front strip)
  mcp.pinMode(LDO3_PIN, OUTPUT);
  mcp.digitalWrite(LDO3_PIN, HIGH);
  delay(5);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();
}

void loop() {
  // Solid red
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
  delay(1000);

  // Solid green
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();
  delay(1000);

  // Solid blue
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
  }
  strip.show();
  delay(1000);

  // Off
  strip.clear();
  strip.show();
  delay(500);
}
