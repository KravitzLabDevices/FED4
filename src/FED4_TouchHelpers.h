#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ESP32-S3 NG touch_sens helpers (latest Arduino-ESP32 / IDF only — no legacy path).
// Counts RISE when touched; values are uint32_t.

#ifndef TOUCH_THRESHOLD
#define TOUCH_THRESHOLD 0.03f
#endif

extern uint32_t fed4TouchIdleL;
extern uint32_t fed4TouchIdleC;
extern uint32_t fed4TouchIdleR;

bool fed4TouchInitPads(void);
uint32_t fed4TouchRead(uint8_t pin);
float fed4TouchRiseFraction(uint32_t raw, uint32_t idle);
uint32_t fed4TouchWakeThreshold(uint32_t idle);
bool fed4TouchPadsReleased(float riseLimit);
bool fed4TouchAnyPadActive(float riseLimit);
bool fed4TouchEnableTouchpadWakeup(void);
/** 0 = none, 1 = left, 2 = center, 3 = right (matches FedPad). */
int fed4TouchIdentifyWakePadIndex(float triggerRise);
/** UT-friendly labels; nullptr if none. */
const char *fed4TouchIdentifyWakePad(float triggerRise);
void fed4TouchPrintDriverConfig(void);

#ifdef __cplusplus
}
#endif
