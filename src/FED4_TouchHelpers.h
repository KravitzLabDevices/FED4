#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ESP32-S3 NG touch_sens helpers (latest Arduino-ESP32 / IDF only — no legacy path).
// Counts RISE when touched; values are uint32_t.

// Caller floor for PadsReleased / identify (optional); per-pad char usually higher.
#ifndef TOUCH_THRESHOLD
#define TOUCH_THRESHOLD 0.03f
#endif

// Characterization window (init / recalibrate)
#ifndef TOUCH_CHAR_WARM_MS
#define TOUCH_CHAR_WARM_MS 400
#endif
#ifndef TOUCH_CHAR_SAMPLES
#define TOUCH_CHAR_SAMPLES 64
#endif
#ifndef TOUCH_CHAR_INTERVAL_MS
#define TOUCH_CHAR_INTERVAL_MS 20
#endif
// Absolute wake/detect delta (counts): max(sigma*std + abs_margin, abs_min)
// Then riseThresh = clamp(absDelta/mean, TOUCH_RISE_MIN, TOUCH_RISE_MAX).
// Absolute (not % of idle) so high-baseline pads (e.g. Left ~220k) stay as
// sensitive as lower-baseline pads for similar poke capacitance.
#ifndef TOUCH_CHAR_SIGMA
#define TOUCH_CHAR_SIGMA 8.0f
#endif
#ifndef TOUCH_CHAR_ABS_MARGIN
#define TOUCH_CHAR_ABS_MARGIN 400.0f
#endif
#ifndef TOUCH_CHAR_ABS_MIN
#define TOUCH_CHAR_ABS_MIN 800.0f
#endif
#ifndef TOUCH_RISE_MIN
#define TOUCH_RISE_MIN 0.005f
#endif
#ifndef TOUCH_RISE_MAX
#define TOUCH_RISE_MAX 0.12f
#endif

extern uint32_t fed4TouchIdleL;
extern uint32_t fed4TouchIdleC;
extern uint32_t fed4TouchIdleR;

extern float fed4TouchStdL;
extern float fed4TouchStdC;
extern float fed4TouchStdR;

/** Per-pad software rise thresholds (fraction of idle). */
extern float fed4TouchRiseThreshL;
extern float fed4TouchRiseThreshC;
extern float fed4TouchRiseThreshR;

bool fed4TouchInitPads(void);
uint32_t fed4TouchRead(uint8_t pin);
float fed4TouchRiseFraction(uint32_t raw, uint32_t idle);
uint32_t fed4TouchWakeThreshold(uint32_t idle);
uint32_t fed4TouchWakeThresholdForPad(uint32_t idle, float riseThresh);

/** True when all pads are below their characterized rise thresholds.
 *  riseLimit: optional stricter (lower) override; ignored if >= per-pad thresh. */
bool fed4TouchPadsReleased(float riseLimit);
bool fed4TouchAnyPadActive(float riseLimit);
bool fed4TouchEnableTouchpadWakeup(void);

/** Full idle characterization: warm → sample mean/std → set idle + rise thresh + HW wake. */
bool fed4TouchCharacterizePads(void);
void fed4TouchPrintCharacterization(void);

/** 0 = none, 1 = left, 2 = center, 3 = right (matches FedPad). */
int fed4TouchIdentifyWakePadIndex(float triggerRise);
/** Map active HW channel → FedPad index (0 if none).
 *  Light sleep: uses on_active latch and/or smooth−benchmark vs wakeAbs
 *  (esp_sleep_get_touchpad_wakeup_status is deep-sleep-only — not used). */
int fed4TouchPadIndexFromHwWakeStatus(void);
/** Clear on_active latch (call before entering light sleep). */
void fed4TouchClearWakePadLatch(void);
/** UT-friendly labels; nullptr if none. */
const char *fed4TouchIdentifyWakePad(float triggerRise);
void fed4TouchPrintDriverConfig(void);

/** waitUntil latency marks (FED4_DIAG_POKE_TIMING). Ids match wiki table. */
enum {
  FED4_POKE_T_WAKE = 0,     // esp_light_sleep_start returned
  FED4_POKE_T_WAKEUP,       // wakeUp() entered
  FED4_POKE_T_PRE_CAPTURE,  // about to call capturePoke()
  FED4_POKE_T_IDENTIFIED,   // pad index known inside capturePoke
  FED4_POKE_T_CAPTURE_DONE, // capturePoke returned (after hold/release)
  FED4_POKE_T_CLASSIFIED,   // FedEvent fields set + redPix
  FED4_POKE_T_LOG_DONE,     // logData returned (or skipped)
  FED4_POKE_T_UPDATE_DONE,  // update() returned
  FED4_POKE_T_COUNT
};
void fed4PokeTimingReset(void);
void fed4PokeTimingMark(int id);
void fed4PokeTimingPrint(const char *note);

#ifdef __cplusplus
}
#endif
