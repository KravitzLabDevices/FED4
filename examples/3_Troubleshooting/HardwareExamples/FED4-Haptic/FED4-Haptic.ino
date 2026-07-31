/*
 * FED4 Haptic Test
 *
 * - Haptic control is MCP23017 pin 5 (EXP_HAPTIC)
 * - Haptic is on PSV2 rail, so EXP_PSV2_EN (MCP pin 13) must be enabled first
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

Adafruit_MCP23X17 mcp;

void buzz(unsigned long onMs, unsigned long offMs) {
  mcp.digitalWrite(EXP_HAPTIC, HIGH);
  delay(onMs);
  mcp.digitalWrite(EXP_HAPTIC, LOW);
  delay(offMs);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Haptic Test ===");

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed.");
    while (1) delay(10);
  }

  // Enable PSV2 rail (haptic is powered from PSV2).
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW); // ~ON active-low
  delay(5);

  // Configure haptic output and start off.
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  Serial.println("Haptic ready.");
}

void loop() {
  Serial.println("Single buzz");
  buzz(120, 500);

  Serial.println("Double buzz");
  buzz(60, 90);
  buzz(60, 500);

  Serial.println("Rumble");
  for (int i = 0; i < 10; i++) {
    buzz(20, 20);
  }
  delay(800);
}