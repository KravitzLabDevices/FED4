/*
 * FED4 LED Position Test
 * 
 * This sketch helps identify which physical LED corresponds to which position number.
 * It will:
 * 1. Light up each LED position (0-7) one at a time in sequence
 * 2. Show the position number in Serial Monitor
 * 3. Use different colors to make identification easier
 * 
 * Watch the LEDs and note which physical LED lights up for each position number.
 */

#include "FED4.h"

FED4 fed4;

static const uint8_t NUM_LEDS = 8;
static const char *kColors[NUM_LEDS] = {
  "red", "green", "blue", "yellow", "cyan", "purple", "orange", "white"
};

void lightSingle(uint8_t idx, const char *color, unsigned long holdMs) {
  fed4.lightsOff();
  fed4.setStripPixel(idx, color);
  Serial.print("LED index ");
  Serial.print(idx);
  Serial.print(" -> ");
  Serial.println(color);
  delay(holdMs);
}

void printMapHint() {
  Serial.println("Library group mapping (from FED4_LEDs.cpp):");
  Serial.println("  Left group:   0,1,2");
  Serial.println("  Center group: 3,4");
  Serial.println("  Right group:  5,6,7");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n=== FED4 LED Position Test ===");
  Serial.println("Initializing FED4...");

  fed4.sleepyLEDs = false;
  fed4.useMotionSensor = false;
  if (!fed4.begin("LEDPositionTest")) {
    Serial.println("ERROR: FED4 begin() failed.");
    while (1) delay(1000);
  }

  fed4.lightsOff();
  printMapHint();
  Serial.println("Starting in 2 seconds...\n");
  delay(2000);
}

void loop() {
  Serial.println("--- Phase 1: White index sweep ---");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    lightSingle(i, "white", 1200);
  }

  Serial.println("--- Phase 2: Color-coded index sweep ---");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    lightSingle(i, kColors[i], 1200);
  }

  Serial.println("--- Phase 3: Fast chase order check ---");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    fed4.lightsOff();
    fed4.setStripPixel(i, "red");
    delay(220);
  }

  Serial.println("--- Phase 4: Group check (L/C/R) ---");
  fed4.leftLight("red");
  Serial.println("Left group lit");
  delay(900);
  fed4.centerLight("green");
  Serial.println("Center group lit");
  delay(900);
  fed4.rightLight("blue");
  Serial.println("Right group lit");
  delay(900);

  fed4.lightsOff();
  Serial.println("--- Cycle complete. Restarting in 2s ---\n");
  delay(2000);
}
