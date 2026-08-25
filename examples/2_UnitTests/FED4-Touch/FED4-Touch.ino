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

  fed4TouchPrintCharacterization();
  fed4TouchPrintDriverConfig();
  Serial.println("FED4 touch — smooth + rise every 100 ms (active if rise>=thresh)");
}

void loop() {
  const uint32_t l = fed4TouchRead(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchRead(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchRead(TOUCH_PAD_RIGHT);
  const float rl = fed4TouchRiseFraction(l, fed4TouchIdleL);
  const float rc = fed4TouchRiseFraction(c, fed4TouchIdleC);
  const float rr = fed4TouchRiseFraction(r, fed4TouchIdleR);

  Serial.printf(
      "L:%lu C:%lu R:%lu | rise L:%.3f%s C:%.3f%s R:%.3f%s | thr L:%.4f C:%.4f R:%.4f\n",
      (unsigned long)l, (unsigned long)c, (unsigned long)r,
      rl, (rl >= fed4TouchRiseThreshL) ? "*" : " ",
      rc, (rc >= fed4TouchRiseThreshC) ? "*" : " ",
      rr, (rr >= fed4TouchRiseThreshR) ? "*" : " ",
      (double)fed4TouchRiseThreshL, (double)fed4TouchRiseThreshC,
      (double)fed4TouchRiseThreshR);

  delay(PRINT_MS);
}
