#pragma once

#include <stdint.h>

// ESP32-S3 touch helpers for standalone FED4 hardware examples.
// IDF 5.5+: direct touch_sens NG driver (see FED4-Touch-Light-Sleep-Multiple bisection notes).
// Counts RISE when touched; values are uint32_t.

extern uint32_t fed4TouchIdleL;
extern uint32_t fed4TouchIdleC;
extern uint32_t fed4TouchIdleR;

bool fed4TouchS3InitPads();
uint32_t fed4TouchS3Read(uint8_t pin);
uint32_t fed4TouchS3ReadSmooth(uint8_t pin);
float fed4TouchS3RiseFraction(uint32_t raw, uint32_t idle);
uint32_t fed4TouchS3WakeThreshold(uint32_t idle);

// Light-sleep examples: configure hardware touch wake after init.
bool fed4TouchS3EnableTouchpadWakeup();

// Returns "LEFT", "CENTER", "RIGHT", or nullptr if no pad exceeds triggerRise.
const char *fed4TouchS3IdentifyWakePad(float triggerRise);

#if defined(ARDUINO) && defined(ESP_PLATFORM)
void fed4TouchS3PrintDriverConfig();
#endif
