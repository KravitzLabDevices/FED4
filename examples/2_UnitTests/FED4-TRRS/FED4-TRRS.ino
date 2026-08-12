/*
 * FED4 TRRS Test
 *
 * TRRS inputs (digital, pull-up — LOW when externally pulled down):
 *   AUDIO_TRRS_1 = 4  -> left strip group (LEDs 0-2), red
 *   AUDIO_TRRS_2 = 5  -> center strip group (LEDs 3-4), green
 *   AUDIO_TRRS_3 = 6  -> right strip group (LEDs 5-7), blue
 *
 * Visual feedback uses the front WS2812 strip on GPIO 36 (PSV3 rail).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <FastLED_NeoPixel.h>
#include <FED4_Pins.h>

#define NUM_LEDS 8
#define STRIP_BRIGHTNESS 40

Adafruit_MCP23X17 mcp;
FastLED_NeoPixel<NUM_LEDS, RGB_STRIP, NEO_GRB> strip;

void showStripForTrrs(int trrs1, int trrs2, int trrs3) {
  strip.clear();

  if (trrs1 == LOW) {
    strip.setPixelColor(0, strip.Color(255, 0, 0));
    strip.setPixelColor(1, strip.Color(255, 0, 0));
    strip.setPixelColor(2, strip.Color(255, 0, 0));
  }
  if (trrs2 == LOW) {
    strip.setPixelColor(3, strip.Color(0, 255, 0));
    strip.setPixelColor(4, strip.Color(0, 255, 0));
  }
  if (trrs3 == LOW) {
    strip.setPixelColor(5, strip.Color(0, 0, 255));
    strip.setPixelColor(6, strip.Color(0, 0, 255));
    strip.setPixelColor(7, strip.Color(0, 0, 255));
  }

  strip.show();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
  pinMode(AUDIO_TRRS_2, INPUT_PULLUP);
  pinMode(AUDIO_TRRS_3, INPUT_PULLUP);

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed — strip may be unpowered.");
  } else {
    mcp.pinMode(EXP_PSV3_EN, OUTPUT);
    mcp.digitalWrite(EXP_PSV3_EN, LOW); // ~ON active-low
    delay(5);
  }

  strip.begin();
  strip.setBrightness(STRIP_BRIGHTNESS);
  strip.clear();
  strip.show();

  Serial.println("=== FED4 TRRS Test ===");
  Serial.println("Strip: TRRS1=left/red, TRRS2=center/green, TRRS3=right/blue");
}

void loop() {
  int trrs1 = digitalRead(AUDIO_TRRS_1);
  int trrs2 = digitalRead(AUDIO_TRRS_2);
  int trrs3 = digitalRead(AUDIO_TRRS_3);

  Serial.print("TRRS1/TRRS2/TRRS3: ");
  Serial.print(trrs1);
  Serial.print(" / ");
  Serial.print(trrs2);
  Serial.print(" / ");
  Serial.println(trrs3);

  showStripForTrrs(trrs1, trrs2, trrs3);
  delay(120);
}
