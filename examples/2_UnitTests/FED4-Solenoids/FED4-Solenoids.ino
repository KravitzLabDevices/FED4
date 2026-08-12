/*
 * FED4 Solenoid Test
 *
 * Alternates EXP_SOL_1 and EXP_SOL_2 on the MCP23017, 500 ms each.
 *   EXP_SOL_1 = MCP pin 2
 *   EXP_SOL_2 = MCP pin 3
 *
 * HIGH energizes a solenoid; both start de-energized (LOW).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

Adafruit_MCP23X17 mcp;

static const unsigned long SOL_MS = 500;

void setSolenoids(bool sol1, bool sol2) {
  mcp.digitalWrite(EXP_SOL_1, sol1 ? HIGH : LOW);
  mcp.digitalWrite(EXP_SOL_2, sol2 ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Solenoid Test ===");
  Serial.println("SOL_1 = MCP2, SOL_2 = MCP3 — alternating 500 ms");

  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed.");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_SOL_1, OUTPUT);
  mcp.pinMode(EXP_SOL_2, OUTPUT);
  setSolenoids(false, false);

  Serial.println("Solenoids ready.");
}

void loop() {
  Serial.println("SOL_1 on");
  setSolenoids(true, false);
  delay(SOL_MS);

  Serial.println("SOL_2 on");
  setSolenoids(false, true);
  delay(SOL_MS);
}
