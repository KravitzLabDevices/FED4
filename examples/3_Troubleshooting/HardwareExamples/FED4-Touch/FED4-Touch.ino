/*
 * FED4 Touch Test (ESP32-S3)
 *
 * Prints raw + smoothed touch values every 100 ms. No LEDs.
 *
 * Touch pads (FED4_Pins.h):
 *   LEFT   = TOUCH_PAD_NUM1 (GPIO 1)
 *   CENTER = TOUCH_PAD_NUM3 (GPIO 3)
 *   RIGHT  = TOUCH_PAD_NUM2 (GPIO 2)
 *
 * Uses shared FED4_TouchS3 helpers (IDF 5.5+ NG direct driver when available).
 * S3 counts RISE when touched; values are uint32_t (not uint16_t).
 */

#include <Arduino.h>
#include <FED4_Pins.h>
#include "../FED4_TouchS3/FED4_TouchS3.h"

static const uint32_t PRINT_MS = 100;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!fed4TouchS3InitPads()) {
    Serial.println("Touch init failed — keep pads clear at boot");
    while (true) delay(10);
  }

  Serial.printf("Idle L:%lu C:%lu R:%lu\n",
                (unsigned long)fed4TouchIdleL, (unsigned long)fed4TouchIdleC,
                (unsigned long)fed4TouchIdleR);
  fed4TouchS3PrintDriverConfig();
  Serial.println("FED4 touch — raw + smooth every 100 ms");
}

void loop() {
  const uint32_t l = fed4TouchS3Read(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchS3Read(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchS3Read(TOUCH_PAD_RIGHT);

  const uint32_t ls = fed4TouchS3ReadSmooth(TOUCH_PAD_LEFT);
  const uint32_t cs = fed4TouchS3ReadSmooth(TOUCH_PAD_CENTER);
  const uint32_t rs = fed4TouchS3ReadSmooth(TOUCH_PAD_RIGHT);

  Serial.printf("raw L:%lu C:%lu R:%lu | smooth L:%lu C:%lu R:%lu\n",
                (unsigned long)l, (unsigned long)c, (unsigned long)r,
                (unsigned long)ls, (unsigned long)cs, (unsigned long)rs);

  delay(PRINT_MS);
}
