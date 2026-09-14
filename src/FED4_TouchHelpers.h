#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ESP32-S3 legacy touch_pad helpers (Arduino-ESP32 3.2.1 / IDF 5.4 — no NG
// touch_sens path compiled in; see docs/wiki/Poke-Functionality.md).
// Counts RISE when touched; values are uint32_t.

// Caller floor for PadsReleased / identify (optional); per-pad char usually higher.
#ifndef TOUCH_THRESHOLD
#define TOUCH_THRESHOLD 0.03f
#endif

// --- Hardware measurement timing --------------------------------------------
// Charge/discharge cycles per measurement (touch_pad_set_charge_discharge_times).
// IDF recommends sizing this so one measurement takes ~1 ms. FED4's original
// value of 2000 measured ~9.7 ms on the largest-baseline pad (Left) — a ~10x
// overshoot that (a) left almost no headroom before a loaded pad's measurement
// ran out of range and stopped the FSM (see fed4TouchOnActive() / the
// touch_pad_timeout_* calls in FED4_Touch.cpp — this is what killed the field
// unit, see docs/wiki/Poke-Functionality.md), and (b) made the confirm window
// and characterization sampling re-read the same slow-changing hardware value
// instead of taking independent samples. 200 targets ~1 ms on Left; verify
// against the measured ScanPeriodUs / MeasUsL touch-log columns and retune if
// your enclosure's baselines differ substantially.
#ifndef TOUCH_MEASURE_CYCLES
#define TOUCH_MEASURE_CYCLES 200
#endif

// Multiplier on TOUCH_MEASURE_CYCLES for the FSM measurement-timeout threshold
// (touch_pad_timeout_set). Expressed as a multiplier so the timeout margin
// (roughly 8x the largest pad's nominal reading) scales automatically if
// TOUCH_MEASURE_CYCLES is retuned. Without a timeout handler at all, an
// out-of-range measurement (e.g. a heavily loaded pad) silently stops the FSM
// forever — see touch_pad_timeout_resume() / fed4TouchServiceTimeout().
#ifndef TOUCH_TIMEOUT_MULTIPLIER
#define TOUCH_TIMEOUT_MULTIPLIER 700
#endif

// Settle time after touch_pad_fsm_start() before the filter is trusted, and
// again after the post-start touch_pad_reset_benchmark() call.
#ifndef FED4_TOUCH_INIT_SETTLE_MS
#define FED4_TOUCH_INIT_SETTLE_MS 100
#endif

// --- Bounded pre-sleep release wait (startSleep()) --------------------------
// Absolute cap: proceed to sleep regardless once exceeded (the timer wake stays
// armed) rather than spinning forever waiting for pads to release.
#ifndef FED4_TOUCH_RELEASE_WAIT_MS
#define FED4_TOUCH_RELEASE_WAIT_MS 5000UL
#endif
// Lower bound for logging a "release was slow but did not hang" row.
#ifndef FED4_TOUCH_RELEASE_LOG_MS
#define FED4_TOUCH_RELEASE_LOG_MS 500UL
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
// Absolute (not % of idle) so high-baseline pads (e.g. Left/GPIO1 ~220k) stay as
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

// Post-wake pad ID confirm (absolute Δ vs idle — not first-cross rise%)
#ifndef TOUCH_ID_CONFIRM_MS
#define TOUCH_ID_CONFIRM_MS 30
#endif
#ifndef TOUCH_ID_CONFIRM_DT_MS
#define TOUCH_ID_CONFIRM_DT_MS 5
#endif
#ifndef TOUCH_ID_CONFIRM_AGREE
#define TOUCH_ID_CONFIRM_AGREE 3
#endif
/** Confirm winner must beat #2 by at least this fraction of winner Δ (0 = off). */
#ifndef TOUCH_ID_CONFIRM_MARGIN
#define TOUCH_ID_CONFIRM_MARGIN 0.25f
#endif

/** Fraction of measured poke delta used for HW/software wakeAbs. */
#ifndef TOUCH_CAL_DELTA_FRAC
#define TOUCH_CAL_DELTA_FRAC 0.4f
#endif

#define FED4_TOUCH_CAL_VER 1

typedef struct
{
  uint32_t idleMean;
  float idleStd;
  uint32_t touchDelta; // touched − idle (counts)
  uint32_t wakeAbs;    // derived absolute active_thresh
  float riseThresh;    // wakeAbs / idleMean
} Fed4TouchCalPad;

typedef struct
{
  uint8_t ver;
  uint8_t mapL;
  uint8_t mapC;
  uint8_t mapR;
  Fed4TouchCalPad L;
  Fed4TouchCalPad C;
  Fed4TouchCalPad R;
  uint32_t unixTime;
} Fed4TouchCal;

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

/** Absolute HW active_thresh currently programmed per pad (counts above benchmark). */
extern uint32_t fed4TouchWakeAbsL;
extern uint32_t fed4TouchWakeAbsC;
extern uint32_t fed4TouchWakeAbsR;

bool fed4TouchInitPads(void);
uint32_t fed4TouchRead(uint8_t pin);
/** NG hardware benchmark (what HW wake compares smooth against — auto-tracks drift). */
uint32_t fed4TouchReadBenchmark(uint8_t pin);
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

/** NVS touch calibration (follows device; schema FED4_TOUCH_CAL_VER). */
void fed4TouchCalSetMap(Fed4TouchCal *cal);
void fed4TouchCalDerivePad(Fed4TouchCalPad *pad);
bool fed4TouchCalValid(const Fed4TouchCal *cal);
bool fed4TouchCalSave(const Fed4TouchCal *cal);
bool fed4TouchCalLoad(Fed4TouchCal *out);
bool fed4TouchCalClear(void);
bool fed4TouchCalApply(const Fed4TouchCal *cal);
void fed4TouchCalPrint(const Fed4TouchCal *cal);

/** 0 = none, 1 = left, 2 = center, 3 = right (matches FedPad). */
int fed4TouchIdentifyWakePadIndex(float triggerRise);
/** Map active HW channel → FedPad index (0 if none).
 *  Light sleep: uses on_active latch and/or smooth−benchmark vs wakeAbs
 *  (esp_sleep_get_touchpad_wakeup_status is deep-sleep-only — not used). */
int fed4TouchPadIndexFromHwWakeStatus(void);
/** Latch only: newly-active status_mask bits accumulated since the last clear
 *  (edge, not level — see fed4TouchOnActive()); falls back to the strongest
 *  live smooth−benchmark delta among the set bits if more than one is set.
 *  Does NOT use touch_pad_get_current_meas_channel() for identification (that
 *  register reports whichever channel the FSM happens to be scanning, not the
 *  channel that went active — see docs/wiki/Poke-Functionality.md). */
int fed4TouchPadIndexFromLatchOnly(void);
/** Sample absolute (smooth−idle) over TOUCH_ID_CONFIRM_* window; 0 if no run
 *  of TOUCH_ID_CONFIRM_AGREE samples agreed (never returns an unvoted single-
 *  sample winner — see fed4TouchLastConfirmAgreed()). */
int fed4TouchConfirmPadByAbsDelta(void);
/** True if the last fed4TouchConfirmPadByAbsDelta() call returned a voted pad
 *  (always true when it returned nonzero, false when it returned 0). */
bool fed4TouchLastConfirmAgreed(void);
/** Last confirm sample's (smooth−idle) delta for the given pad (1/2/3), 0 else. */
int32_t fed4TouchLastConfirmDelta(int padIndex);
/** Clear on_active latch (call before entering light sleep). */
void fed4TouchClearWakePadLatch(void);
/** UT-friendly labels; nullptr if none. */
const char *fed4TouchIdentifyWakePad(float triggerRise);
void fed4TouchPrintDriverConfig(void);

// --- Diagnostics: ISR / timing (touch drift log) -----------------------------
/** Live touch_pad_get_status() poll (bit N = channel N currently active). */
uint32_t fed4TouchLiveStatusMask(void);
/** Raw touch_pad_get_current_meas_channel() at the last ISR entry — diagnostic
 *  only, never used for pad identification (see fed4TouchPadIndexFromLatchOnly). */
int fed4TouchLastIsrChan(void);
/** Edge-accumulated active mask at the last consumer read (pre-clear). */
uint32_t fed4TouchLastIsrMask(void);
/** Total ACTIVE/INACTIVE/TIMEOUT ISR entries since boot (0 means the ISR never
 *  fired at all — distinguishes "ISR silent" from "ISR reports wrong channel"). */
uint32_t fed4TouchIsrCount(void);
/** FSM measurement-timeout occurrences since boot (see touch_pad_timeout_set). */
uint32_t fed4TouchTimeoutCount(void);
/** Services a pending measurement-timeout by calling touch_pad_timeout_resume()
 *  from task context. Cheap (a volatile bool check) — call it from any touch
 *  read path; fed4TouchRead()/fed4TouchReadBenchmark() already do. */
void fed4TouchServiceTimeout(void);
/** Measured full touch scan cycle time (µs), probed once at init. */
uint32_t fed4TouchScanPeriodUs(void);
/** Measured per-channel measurement time (µs) for pad 1/2/3, probed at init. */
uint32_t fed4TouchMeasUs(int padIndex);
/** Per-pad peak smooth seen during the last capturePoke() hold, regardless of
 *  which pad was resolved as the poke (independent ground truth for scoring
 *  LatchPad/ConfirmPad/Pad against each other offline). */
uint32_t fed4TouchLastPeakL(void);
uint32_t fed4TouchLastPeakC(void);
uint32_t fed4TouchLastPeakR(void);

// --- Diagnostic snapshots (touch drift log; see FED4_SD.cpp logTouch) ---------
/** Latch decision from the last capturePoke() (0 = none). */
int fed4TouchLastLatchPad(void);
/** Absolute-Δ confirm decision from the last capturePoke() (0 = none). */
int fed4TouchLastConfirmPad(void);
/** Peak smooth count seen on the resolved pad during the last poke hold. */
uint32_t fed4TouchLastPeakSmooth(void);
/** Times fed4TouchCharacterizePads() ran from the startSleep() rescue path. */
uint32_t fed4TouchRecharCount(void);
/** Called by the startSleep() rescue path — do not call from sketches. */
void fed4TouchNoteRechar(void);
/** Publish a software-detected poke (awake / bench arms have no capturePoke()). */
void fed4TouchSetLastPoke(int latchPad, int confirmPad, uint32_t peakSmooth);

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
