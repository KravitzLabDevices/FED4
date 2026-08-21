/*
 * FED4 Touch Test (ESP32-S3)
 *
 * Prints smooth touch counts + rise fraction every 100 ms. No LEDs.
 *
 * Touch pads (FED4_Pins.h):
 *   LEFT   = GPIO 1
 *   CENTER = GPIO 3
 *   RIGHT  = GPIO 2
 *
 * Uses library helpers (FED4_TouchHelpers.h) — NG touch_sens, uint32_t rise.
 */

#include <Arduino.h>
#include <FED4_Pins.h>
#include <FED4_TouchHelpers.h>

static const uint32_t PRINT_MS = 100;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!fed4TouchInitPads()) {
    Serial.println("Touch init failed — keep pads clear at boot");
    while (true) delay(10);
  }

  Serial.printf("Idle L:%lu C:%lu R:%lu\n",
                (unsigned long)fed4TouchIdleL, (unsigned long)fed4TouchIdleC,
                (unsigned long)fed4TouchIdleR);
  fed4TouchPrintDriverConfig();
  Serial.println("FED4 touch — smooth + rise every 100 ms");
}

void loop() {
  const uint32_t l = fed4TouchRead(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchRead(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchRead(TOUCH_PAD_RIGHT);

  Serial.printf("L:%lu C:%lu R:%lu | rise L:%.3f C:%.3f R:%.3f\n",
                (unsigned long)l, (unsigned long)c, (unsigned long)r,
                fed4TouchRiseFraction(l, fed4TouchIdleL),
                fed4TouchRiseFraction(c, fed4TouchIdleC),
                fed4TouchRiseFraction(r, fed4TouchIdleR));

  delay(PRINT_MS);
}
