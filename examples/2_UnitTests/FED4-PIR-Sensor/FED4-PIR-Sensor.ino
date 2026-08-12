/*
 * FED4 PIR Motion Sensor Test
 *
 * EKMB1107112 PIR on GPIO 10 (PIR_MOTION), INPUT_PULLDOWN.
 * HIGH = motion detected. STATUS_LED mirrors the PIR logic level.
 *
 * Pins (see src/FED4_Pins.h):
 *   PIR_MOTION = 10
 *   STATUS_LED = 35
 */

#include <Arduino.h>
#include <FED4_Pins.h>

int lastPir = -1;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(PIR_MOTION, INPUT_PULLDOWN);
  pinMode(STATUS_LED, OUTPUT);
  analogWrite(STATUS_LED, 0);

  Serial.println("=== FED4 PIR Motion Test ===");
  Serial.printf("PIR_MOTION: GPIO %d (INPUT_PULLDOWN)\n", PIR_MOTION);
  Serial.printf("STATUS_LED: GPIO %d (mirrors PIR level)\n", STATUS_LED);
}

void loop() {
  int pir = digitalRead(PIR_MOTION);

  analogWrite(STATUS_LED, pir ? 255 : 0);

  if (pir != lastPir) {
    Serial.printf("PIR_MOTION: %s (%d)\n", pir ? "HIGH (motion)" : "LOW (clear)", pir);
    lastPir = pir;
  }

  delay(50);
}
