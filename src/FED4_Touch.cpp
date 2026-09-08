#include "FED4.h"
#include "FED4_TouchHelpers.h"

#include <math.h>
#include "driver/touch_sensor.h"

// ---------------------------------------------------------------------------
// Legacy touch_pad driver (Arduino-ESP32 3.2.1 / IDF 5.4 — no NG touch_sens)
// ---------------------------------------------------------------------------

// TOUCH_MEASURE_CYCLES is defined in FED4_TouchHelpers.h (charge/discharge
// cycles per measurement — sized for ~1 ms measurements; see that header).
// This is the RTC_SLOW-clocked idle gap BETWEEN measurements, not the total
// scan-cycle time — that is dominated by TOUCH_MEASURE_CYCLES and is on the
// order of a few ms; see the measured ScanPeriodUs/MeasUs{L,C,R} touch-log
// columns (fed4TouchScanPeriodUs()/fed4TouchMeasUs()) for the actual value.
static const uint16_t TOUCH_MEAS_INTERVAL_CYCLES = 4;

uint32_t fed4TouchIdleL = 0;
uint32_t fed4TouchIdleC = 0;
uint32_t fed4TouchIdleR = 0;

float fed4TouchStdL = 0.0f;
float fed4TouchStdC = 0.0f;
float fed4TouchStdR = 0.0f;

float fed4TouchRiseThreshL = TOUCH_THRESHOLD;
float fed4TouchRiseThreshC = TOUCH_THRESHOLD;
float fed4TouchRiseThreshR = TOUCH_THRESHOLD;

uint32_t fed4TouchWakeAbsL = 0;
uint32_t fed4TouchWakeAbsC = 0;
uint32_t fed4TouchWakeAbsR = 0;

uint8_t FED4::wakePad = 0; // 0=none, 1=left, 2=center, 3=right

static bool sTouchInited = false;
static bool sTouchChanEnabled[TOUCH_PAD_MAX] = {};

// Light sleep: esp_sleep_get_touchpad_wakeup_status() is deep-sleep-only.
// sTouchActiveMask is an EDGE accumulator (bits newly active since the last
// consumer clear), not a level snapshot — see fed4TouchOnActive(). sTouchPrevStatus
// is the ISR's own running level, used only to compute that edge.
static volatile uint32_t sTouchActiveMask = 0;
static volatile uint32_t sTouchPrevStatus = 0;
static volatile int sTouchActiveChanId = -1; // diagnostic only — see header note
static int sTouchLastResolvedChan = -1;      // for POKE_TIMING (survives latch clear)

// ISR/timeout counters (diagnostic; touch drift CSV).
static volatile uint32_t sTouchIsrCount = 0;
static volatile uint32_t sTouchIsrActiveCount = 0;
static volatile uint32_t sTouchTimeoutCount = 0;
static volatile bool sTouchTimeoutPending = false;

// Diagnostic snapshots consumed by FED4::logTouch() (touch drift CSV).
static int sTouchLastLatchPad = 0;
static int sTouchLastConfirmPad = 0;
static bool sTouchLastConfirmAgreed = false;
static int32_t sTouchLastConfirmDeltaL = 0;
static int32_t sTouchLastConfirmDeltaC = 0;
static int32_t sTouchLastConfirmDeltaR = 0;
static uint32_t sTouchLastPeakSmooth = 0;
static uint32_t sTouchLastPeakL = 0;
static uint32_t sTouchLastPeakC = 0;
static uint32_t sTouchLastPeakR = 0;
static uint32_t sTouchRecharCount = 0;
static bool sTouchLastCalRejectPending = false;

// Measured scan timing (fed4TouchMeasureScanTiming(), probed once at init).
static uint32_t sScanPeriodUs = 0;
static uint32_t sMeasUsL = 0, sMeasUsC = 0, sMeasUsR = 0;

// Boot-reference characterization (latched once, from the first successful
// characterization) — plausibility bound for every later re-characterization.
// See TOUCH_CAL_MAX_IDLE_DEVIATION_FRAC / TOUCH_CAL_MAX_RISE_MULT.
static uint32_t sTouchIdleBootL = 0, sTouchIdleBootC = 0, sTouchIdleBootR = 0;
static float sTouchRiseThreshBootL = 0.0f, sTouchRiseThreshBootC = 0.0f,
             sTouchRiseThreshBootR = 0.0f;

// Stale-benchmark watchdog state (Heartbeat-path only).
static uint32_t sBenchStuckSinceMsL = 0, sBenchStuckSinceMsC = 0, sBenchStuckSinceMsR = 0;

/**
 * ISR: latches the newly-active status_mask edge, not the "current measure
 * channel" register. touch_pad_get_current_meas_channel() reports whichever
 * channel the FSM happens to be scanning at ISR-entry time, not the channel
 * that triggered ACTIVE — against ms-scale measurements the FSM has almost
 * always already advanced to the next channel by the time the ISR runs, which
 * silently remapped Left→Right→Center (see docs/wiki/Poke-Functionality.md).
 * touch_pad_get_status() is the correct, authoritative level register; we XOR
 * it against our own running copy to get the edge, then OR that into the
 * consumer-facing accumulator so multiple edges between reads are not lost.
 * Also handles INACTIVE (to keep the running level current so a
 * release-then-repress on the same channel is seen as a fresh edge) and
 * TIMEOUT (a runaway/out-of-range measurement — see fed4TouchServiceTimeout()).
 *
 * IDF rtc_isr dispatcher is assumed to clear RTCCNTL.int_st after dispatch
 * (Arduino HAL never calls touch_pad_intr_clear either). If the ISR re-fires
 * continuously on hardware, add touch_pad_intr_clear(TOUCH_PAD_INTR_MASK_ACTIVE).
 */
static void IRAM_ATTR fed4TouchOnActive(void *)
{
  sTouchIsrCount++;
  const uint32_t mask = touch_pad_read_intr_status_mask();

  if (mask & TOUCH_PAD_INTR_MASK_TIMEOUT)
  {
    sTouchTimeoutCount++;
    sTouchTimeoutPending = true;
  }

  if (mask & (TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE))
  {
    const uint32_t status = touch_pad_get_status();
    if (mask & TOUCH_PAD_INTR_MASK_ACTIVE)
    {
      sTouchActiveMask |= (status & ~sTouchPrevStatus);
      sTouchIsrActiveCount++;
    }
    sTouchPrevStatus = status;
    sTouchActiveChanId = (int)touch_pad_get_current_meas_channel(); // diagnostic only
  }
}

static void fed4TouchClearActiveLatch(void)
{
  sTouchActiveChanId = -1;
  sTouchActiveMask = 0;
}

void fed4TouchServiceTimeout(void)
{
  if (!sTouchTimeoutPending)
    return;
  sTouchTimeoutPending = false;
  touch_pad_timeout_resume();
  Serial.println("Touch: measurement timeout — FSM resumed");
}

uint32_t fed4TouchLiveStatusMask(void) { return touch_pad_get_status(); }
int fed4TouchLastIsrChan(void) { return sTouchActiveChanId; }
uint32_t fed4TouchLastIsrMask(void) { return sTouchActiveMask; }
uint32_t fed4TouchIsrCount(void) { return sTouchIsrCount; }
uint32_t fed4TouchTimeoutCount(void) { return sTouchTimeoutCount; }
uint32_t fed4TouchScanPeriodUs(void) { return sScanPeriodUs; }
uint32_t fed4TouchLastPeakL(void) { return sTouchLastPeakL; }
uint32_t fed4TouchLastPeakC(void) { return sTouchLastPeakC; }
uint32_t fed4TouchLastPeakR(void) { return sTouchLastPeakR; }
bool fed4TouchLastConfirmAgreed(void) { return sTouchLastConfirmAgreed; }

uint32_t fed4TouchMeasUs(int padIndex)
{
  switch (padIndex)
  {
  case 1:
    return sMeasUsL;
  case 2:
    return sMeasUsC;
  case 3:
    return sMeasUsR;
  default:
    return 0;
  }
}

int32_t fed4TouchLastConfirmDelta(int padIndex)
{
  switch (padIndex)
  {
  case 1:
    return sTouchLastConfirmDeltaL;
  case 2:
    return sTouchLastConfirmDeltaC;
  case 3:
    return sTouchLastConfirmDeltaR;
  default:
    return 0;
  }
}

bool fed4TouchCalRejectPending(void) { return sTouchLastCalRejectPending; }
void fed4TouchClearCalRejectPending(void) { sTouchLastCalRejectPending = false; }

static touch_filter_config_t fed4TouchFilterConfig()
{
  // Prefer edge speed over heavy averaging — software rise uses smooth/raw
  // vs characterized idle; HW wake still has BM + abs thresh for noise.
  // smh_lvl OFF == NG TOUCH_SMOOTH_NO_FILTER (smooth equals raw).
  // active_hysteresis has no v2 equivalent and is dropped.
  touch_filter_config_t cfg = {};
  cfg.mode = TOUCH_PAD_FILTER_IIR_8;
  cfg.debounce_cnt = 1;
  cfg.noise_thr = 2;
  cfg.jitter_step = 0;
  cfg.smh_lvl = TOUCH_PAD_SMOOTH_OFF;
  return cfg;
}

/**
 * Read-only probe: watches touch_pad_get_current_meas_channel() advance for
 * ~200 ms and time-stamps each transition, giving the actual per-channel
 * measurement time and total scan-cycle period. Touches no configuration.
 * Run once at the end of controller creation so TOUCH_MEASURE_CYCLES tuning
 * (and any future retune) is verifiable from the touch drift log instead of
 * assumed — see docs/wiki/Poke-Functionality.md for why the original 22 ms
 * scan (vs. an assumed ~few hundred µs) broke several downstream assumptions.
 */
static void fed4TouchMeasureScanTiming(void)
{
  uint32_t sumUs[TOUCH_PAD_MAX] = {};
  uint32_t cntUs[TOUCH_PAD_MAX] = {};

  touch_pad_t lastChan = touch_pad_get_current_meas_channel();
  uint32_t lastUs = micros();
  const uint32_t probeStart = lastUs;
  const uint32_t probeDurationUs = 200000;

  while ((uint32_t)(micros() - probeStart) < probeDurationUs)
  {
    const touch_pad_t chan = touch_pad_get_current_meas_channel();
    if (chan != lastChan)
    {
      const uint32_t nowUs = micros();
      if (lastChan >= 0 && lastChan < TOUCH_PAD_MAX)
      {
        sumUs[lastChan] += (nowUs - lastUs);
        cntUs[lastChan]++;
      }
      lastChan = chan;
      lastUs = nowUs;
    }
  }

  uint32_t total = 0;
  for (int i = 0; i < TOUCH_PAD_MAX; i++)
  {
    if (cntUs[i])
      sumUs[i] /= cntUs[i]; // mean measured time for that channel
    total += sumUs[i];
  }

  sMeasUsL = sumUs[TOUCH_PAD_LEFT];
  sMeasUsC = sumUs[TOUCH_PAD_CENTER];
  sMeasUsR = sumUs[TOUCH_PAD_RIGHT];
  sScanPeriodUs = total;
}

static bool fed4TouchCreateController()
{
  if (touch_pad_init() != ESP_OK)
    return false;

  touch_pad_set_charge_discharge_times(TOUCH_MEASURE_CYCLES);
  touch_pad_set_measurement_interval(TOUCH_MEAS_INTERVAL_CYCLES);
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);
  touch_pad_set_idle_channel_connect(TOUCH_PAD_CONN_GND);

  touch_pad_denoise_t denoise = {};
  denoise.grade = TOUCH_PAD_DENOISE_BIT4;
  denoise.cap_level = TOUCH_PAD_DENOISE_CAP_L4;
  if (touch_pad_denoise_set_config(&denoise) != ESP_OK)
    return false;
  if (touch_pad_denoise_enable() != ESP_OK)
    return false;

  const int pads[] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  for (int i = 0; i < 3; i++)
  {
    if (touch_pad_config((touch_pad_t)pads[i]) != ESP_OK)
      return false;
    sTouchChanEnabled[pads[i]] = true;
  }

  if (touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER) != ESP_OK)
    return false;

  // Configure the filter BEFORE starting the FSM. touch_pad_fsm_start() seeds
  // every channel's benchmark from its very first — unfiltered, unsettled —
  // measurement period; on the largest-baseline pad (Left) that seed has been
  // observed ~20% low, which then latches the channel permanently active (see
  // docs/wiki/Poke-Functionality.md). Configuring the filter first means the
  // seed is at least filtered; the warm-up + explicit reset below discards it
  // regardless, seeding instead from settled post-filter data.
  touch_filter_config_t filter_cfg = fed4TouchFilterConfig();
  if (touch_pad_filter_set_config(&filter_cfg) != ESP_OK)
    return false;
  if (touch_pad_filter_enable() != ESP_OK)
    return false;

  // Guard against a runaway measurement (a heavily loaded pad reading far
  // outside normal range) stopping the FSM permanently — see fed4TouchOnActive()
  // and fed4TouchServiceTimeout(). Threshold scales with TOUCH_MEASURE_CYCLES.
  if (touch_pad_timeout_set(
          true, (uint32_t)TOUCH_MEASURE_CYCLES * TOUCH_TIMEOUT_MULTIPLIER) !=
      ESP_OK)
    return false;

  if (touch_pad_fsm_start() != ESP_OK)
    return false;

  // Let the (now correctly configured) filter settle, then force-seed the
  // benchmark from settled data instead of trusting the FSM's own seed.
  delay(FED4_TOUCH_INIT_SETTLE_MS);
  for (int i = 0; i < 3; i++)
    touch_pad_reset_benchmark((touch_pad_t)pads[i]);
  delay(FED4_TOUCH_INIT_SETTLE_MS);

  // Do not call touch_pad_sleep_channel_enable() — that is the deep-sleep
  // single-channel path and would break multi-pad light-sleep wake.
  const touch_pad_intr_mask_t intrMask = (touch_pad_intr_mask_t)(
      TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE |
      TOUCH_PAD_INTR_MASK_TIMEOUT);
  if (touch_pad_isr_register(fed4TouchOnActive, NULL, intrMask) != ESP_OK)
    return false;
  if (touch_pad_intr_enable(intrMask) != ESP_OK)
    return false;

  fed4TouchMeasureScanTiming();
  return true;
}

static bool fed4TouchApplyThresholds(uint32_t threshL, uint32_t threshC,
                                     uint32_t threshR)
{
  const int pads[] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t thresh[] = {threshL, threshC, threshR};

  fed4TouchWakeAbsL = threshL;
  fed4TouchWakeAbsC = threshC;
  fed4TouchWakeAbsR = threshR;

  for (int i = 0; i < 3; i++)
  {
    if (touch_pad_set_thresh((touch_pad_t)pads[i], thresh[i]) != ESP_OK)
      return false;
    // Per-channel reset (touch_sensor.h) does NOT stop/restart the FSM — unlike
    // touch_pad_fsm_start(), which would reset every channel's benchmark. Reset
    // here so a fresh threshold starts from a fresh benchmark instead of
    // whatever the hardware happened to settle (or get stuck) on.
    touch_pad_reset_benchmark((touch_pad_t)pads[i]);
  }
  return true;
}

static uint32_t fed4TouchReadChannel(uint8_t pin, bool benchmark)
{
  // Cheap (a volatile bool check) on the common path — services a pending FSM
  // measurement-timeout from task context so a runaway reading resumes instead
  // of freezing every channel forever. Placed here so it runs regardless of
  // which arm/sketch is calling in (characterization, confirm, pads-released,
  // the poke hold loop, logTouch, or a bench sketch's own polling loop).
  fed4TouchServiceTimeout();

  const int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0 || pad >= TOUCH_PAD_MAX || !sTouchChanEnabled[pad])
    return 0;

  uint32_t value = 0;
  const esp_err_t err =
      benchmark ? touch_pad_read_benchmark((touch_pad_t)pad, &value)
                : touch_pad_filter_read_smooth((touch_pad_t)pad, &value);
  if (err != ESP_OK)
    return 0;
  return value;
}

typedef struct
{
  uint32_t mean;
  float stddev;
  float riseThresh;
} Fed4TouchPadStats;

static float clampf(float v, float lo, float hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static void characterizeAllPads(Fed4TouchPadStats *outL, Fed4TouchPadStats *outC,
                                Fed4TouchPadStats *outR)
{
  uint32_t samplesL[TOUCH_CHAR_SAMPLES];
  uint32_t samplesC[TOUCH_CHAR_SAMPLES];
  uint32_t samplesR[TOUCH_CHAR_SAMPLES];
  uint64_t sumL = 0, sumC = 0, sumR = 0;

  for (int i = 0; i < TOUCH_CHAR_SAMPLES; i++)
  {
    samplesL[i] = fed4TouchRead(TOUCH_PAD_LEFT);
    samplesC[i] = fed4TouchRead(TOUCH_PAD_CENTER);
    samplesR[i] = fed4TouchRead(TOUCH_PAD_RIGHT);
    sumL += samplesL[i];
    sumC += samplesC[i];
    sumR += samplesR[i];
    delay(TOUCH_CHAR_INTERVAL_MS);
  }

  auto finishPad = [](uint32_t *samples, uint64_t sum, Fed4TouchPadStats *out) {
    const double mean = (double)sum / (double)TOUCH_CHAR_SAMPLES;
    double varAcc = 0.0;
    for (int i = 0; i < TOUCH_CHAR_SAMPLES; i++)
    {
      const double d = (double)samples[i] - mean;
      varAcc += d * d;
    }
    const float stddev = (float)sqrt(varAcc / (double)TOUCH_CHAR_SAMPLES);
    // Absolute delta from noise — equal poke capacitance → similar wake ease
    // across pads with very different baselines (often Left/GPIO1 ~220k vs C/R ~130–140k).
    float absDelta = TOUCH_CHAR_SIGMA * stddev + TOUCH_CHAR_ABS_MARGIN;
    if (absDelta < TOUCH_CHAR_ABS_MIN)
      absDelta = TOUCH_CHAR_ABS_MIN;
    const float rise =
        (mean > 0.0) ? (absDelta / (float)mean) : TOUCH_RISE_MIN;
    out->mean = (uint32_t)(mean + 0.5);
    out->stddev = stddev;
    out->riseThresh = clampf(rise, TOUCH_RISE_MIN, TOUCH_RISE_MAX);
  };

  finishPad(samplesL, sumL, outL);
  finishPad(samplesC, sumC, outC);
  finishPad(samplesR, sumR, outR);
}

/**
 * True if a candidate (idle, riseThresh) is within TOUCH_CAL_MAX_* of the boot
 * reference. bootIdle == 0 means no reference yet (first-ever characterization)
 * and always passes. See docs/wiki/Poke-Functionality.md: without this bound, a
 * characterization run while a pad was loaded accepted a >13x jump in Idle in
 * the field, which made that pad permanently unresponsive.
 */
static bool fed4TouchWithinCalBounds(uint32_t bootIdle, float bootRise,
                                     uint32_t candidateIdle, float candidateRise)
{
  if (!bootIdle)
    return true;
  const float lo = (float)bootIdle * (1.0f - TOUCH_CAL_MAX_IDLE_DEVIATION_FRAC);
  const float hi = (float)bootIdle * (1.0f + TOUCH_CAL_MAX_IDLE_DEVIATION_FRAC);
  if ((float)candidateIdle < lo || (float)candidateIdle > hi)
    return false;
  if (candidateRise > bootRise * TOUCH_CAL_MAX_RISE_MULT)
    return false;
  return true;
}

static bool fed4TouchAbsorbResidualOffset(void)
{
  // If a pad still looks "active" vs the new mean (settle after sampling),
  // lift idle to the current reading so release-wait cannot hang. Bounded
  // against the boot reference (F3): a residual this large is the same
  // "characterizing while loaded" failure mode as fed4TouchCharacterizePads()
  // itself, and must not be absorbed silently.
  const uint8_t pads[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  uint32_t *idles[3] = {&fed4TouchIdleL, &fed4TouchIdleC, &fed4TouchIdleR};
  float *threshs[3] = {&fed4TouchRiseThreshL, &fed4TouchRiseThreshC,
                       &fed4TouchRiseThreshR};
  const uint32_t bootIdles[3] = {sTouchIdleBootL, sTouchIdleBootC, sTouchIdleBootR};
  const float bootRises[3] = {sTouchRiseThreshBootL, sTouchRiseThreshBootC,
                              sTouchRiseThreshBootR};
  bool adjusted = false;

  for (int i = 0; i < 3; i++)
  {
    const uint32_t raw = fed4TouchRead(pads[i]);
    const float rise = fed4TouchRiseFraction(raw, *idles[i]);
    if (rise >= *threshs[i] && raw > *idles[i])
    {
      if (!fed4TouchWithinCalBounds(bootIdles[i], bootRises[i], raw, *threshs[i]))
      {
        Serial.printf(
            "Touch: pad %d residual rise=%.3f raw=%lu rejected (boot idle=%lu, "
            "bound=±%.0f%%) — NOT absorbing\n",
            pads[i], (double)rise, (unsigned long)raw, (unsigned long)bootIdles[i],
            (double)(TOUCH_CAL_MAX_IDLE_DEVIATION_FRAC * 100.0f));
        sTouchLastCalRejectPending = true;
        continue;
      }
      Serial.printf("Touch: pad %d residual rise=%.3f — absorbing into idle (%lu → %lu)\n",
                    pads[i], (double)rise, (unsigned long)*idles[i],
                    (unsigned long)raw);
      *idles[i] = raw;
      adjusted = true;
    }
  }
  return adjusted;
}

/** Warm filter, characterize mean/std, set per-pad rise thresh + HW wake.
 *  Each pad's new idle/riseThresh is bounded against the boot-reference
 *  characterization (fed4TouchWithinCalBounds) — a rejected pad keeps its
 *  previous value and a CalReject condition is raised for the caller to log. */
bool fed4TouchCharacterizePads(void)
{
  Serial.println("Touch: characterizing baselines (keep pads clear)...");
  Serial.flush();

  const uint32_t warmStart = millis();
  while ((millis() - warmStart) < TOUCH_CHAR_WARM_MS)
  {
    fed4TouchRead(TOUCH_PAD_LEFT);
    fed4TouchRead(TOUCH_PAD_CENTER);
    fed4TouchRead(TOUCH_PAD_RIGHT);
    delay(10);
  }

  Fed4TouchPadStats statsL = {};
  Fed4TouchPadStats statsC = {};
  Fed4TouchPadStats statsR = {};
  characterizeAllPads(&statsL, &statsC, &statsR);

  if (!statsL.mean || !statsC.mean || !statsR.mean)
    return false;

  const bool okL = fed4TouchWithinCalBounds(sTouchIdleBootL, sTouchRiseThreshBootL,
                                            statsL.mean, statsL.riseThresh);
  const bool okC = fed4TouchWithinCalBounds(sTouchIdleBootC, sTouchRiseThreshBootC,
                                            statsC.mean, statsC.riseThresh);
  const bool okR = fed4TouchWithinCalBounds(sTouchIdleBootR, sTouchRiseThreshBootR,
                                            statsR.mean, statsR.riseThresh);

  if (!okL || !okC || !okR)
  {
    Serial.printf(
        "Touch: characterization rejected (boot-bound exceeded) L=%d(%lu/%.4f) "
        "C=%d(%lu/%.4f) R=%d(%lu/%.4f) — keeping previous value(s)\n",
        (int)!okL, (unsigned long)statsL.mean, (double)statsL.riseThresh,
        (int)!okC, (unsigned long)statsC.mean, (double)statsC.riseThresh,
        (int)!okR, (unsigned long)statsR.mean, (double)statsR.riseThresh);
    sTouchLastCalRejectPending = true;
  }

  if (okL)
  {
    fed4TouchIdleL = statsL.mean;
    fed4TouchStdL = statsL.stddev;
    fed4TouchRiseThreshL = statsL.riseThresh;
  }
  if (okC)
  {
    fed4TouchIdleC = statsC.mean;
    fed4TouchStdC = statsC.stddev;
    fed4TouchRiseThreshC = statsC.riseThresh;
  }
  if (okR)
  {
    fed4TouchIdleR = statsR.mean;
    fed4TouchStdR = statsR.stddev;
    fed4TouchRiseThreshR = statsR.riseThresh;
  }

  if (fed4TouchAbsorbResidualOffset())
  {
    (void)fed4TouchAbsorbResidualOffset();
  }

  fed4TouchPrintCharacterization();

  const bool applied = fed4TouchApplyThresholds(
      fed4TouchWakeThresholdForPad(fed4TouchIdleL, fed4TouchRiseThreshL),
      fed4TouchWakeThresholdForPad(fed4TouchIdleC, fed4TouchRiseThreshC),
      fed4TouchWakeThresholdForPad(fed4TouchIdleR, fed4TouchRiseThreshR));

  // Latch the boot reference exactly once, from the first accepted
  // characterization (guaranteed accepted above, since bootIdle==0 passes).
  if (!sTouchIdleBootL)
  {
    sTouchIdleBootL = fed4TouchIdleL;
    sTouchRiseThreshBootL = fed4TouchRiseThreshL;
  }
  if (!sTouchIdleBootC)
  {
    sTouchIdleBootC = fed4TouchIdleC;
    sTouchRiseThreshBootC = fed4TouchRiseThreshC;
  }
  if (!sTouchIdleBootR)
  {
    sTouchIdleBootR = fed4TouchIdleR;
    sTouchRiseThreshBootR = fed4TouchRiseThreshR;
  }

  return applied;
}

void fed4TouchPrintCharacterization(void)
{
  const uint32_t wakeL =
      fed4TouchWakeThresholdForPad(fed4TouchIdleL, fed4TouchRiseThreshL);
  const uint32_t wakeC =
      fed4TouchWakeThresholdForPad(fed4TouchIdleC, fed4TouchRiseThreshC);
  const uint32_t wakeR =
      fed4TouchWakeThresholdForPad(fed4TouchIdleR, fed4TouchRiseThreshR);
  Serial.printf(
      "Touch char L: idle=%lu std=%.1f riseThresh=%.4f wakeAbs=%lu\n",
      (unsigned long)fed4TouchIdleL, (double)fed4TouchStdL,
      (double)fed4TouchRiseThreshL, (unsigned long)wakeL);
  Serial.printf(
      "Touch char C: idle=%lu std=%.1f riseThresh=%.4f wakeAbs=%lu\n",
      (unsigned long)fed4TouchIdleC, (double)fed4TouchStdC,
      (double)fed4TouchRiseThreshC, (unsigned long)wakeC);
  Serial.printf(
      "Touch char R: idle=%lu std=%.1f riseThresh=%.4f wakeAbs=%lu\n",
      (unsigned long)fed4TouchIdleR, (double)fed4TouchStdR,
      (double)fed4TouchRiseThreshR, (unsigned long)wakeR);

  // Bench vs Idle right after characterization — a seed mis-set at FSM start
  // (F2/F6) or a benchmark already stuck from a prior session is visible here
  // at t=0 instead of only inferable later from a frozen Heartbeat run.
  const uint32_t bmL = fed4TouchReadBenchmark(TOUCH_PAD_LEFT);
  const uint32_t bmC = fed4TouchReadBenchmark(TOUCH_PAD_CENTER);
  const uint32_t bmR = fed4TouchReadBenchmark(TOUCH_PAD_RIGHT);
  Serial.printf(
      "Touch char bench-vs-idle: L bench=%lu (Δ%ld) C bench=%lu (Δ%ld) R bench=%lu (Δ%ld)\n",
      (unsigned long)bmL, (long)bmL - (long)fed4TouchIdleL, (unsigned long)bmC,
      (long)bmC - (long)fed4TouchIdleC, (unsigned long)bmR,
      (long)bmR - (long)fed4TouchIdleR);
  Serial.flush();
}

// ---------------------------------------------------------------------------
// NVS touch calibration
// ---------------------------------------------------------------------------

static const char *kTouchCalVerKey = "tchVer";
static const char *kTouchCalMapKey = "tchMap";
static const char *kTouchCalBlobKey = "tchBlob";

void fed4TouchCalSetMap(Fed4TouchCal *cal)
{
  if (!cal)
    return;
  cal->mapL = (uint8_t)TOUCH_PAD_LEFT;
  cal->mapC = (uint8_t)TOUCH_PAD_CENTER;
  cal->mapR = (uint8_t)TOUCH_PAD_RIGHT;
}

void fed4TouchCalDerivePad(Fed4TouchCalPad *pad)
{
  if (!pad || !pad->idleMean)
  {
    if (pad)
    {
      pad->wakeAbs = 0;
      pad->riseThresh = TOUCH_RISE_MIN;
    }
    return;
  }
  float wake = TOUCH_CAL_DELTA_FRAC * (float)pad->touchDelta;
  if (wake < TOUCH_CHAR_ABS_MIN)
    wake = TOUCH_CHAR_ABS_MIN;
  const float wakeMax = (float)pad->idleMean * TOUCH_RISE_MAX;
  if (wake > wakeMax)
    wake = wakeMax;
  pad->wakeAbs = (uint32_t)(wake + 0.5f);
  pad->riseThresh = (float)pad->wakeAbs / (float)pad->idleMean;
  if (pad->riseThresh < TOUCH_RISE_MIN)
    pad->riseThresh = TOUCH_RISE_MIN;
  if (pad->riseThresh > TOUCH_RISE_MAX)
    pad->riseThresh = TOUCH_RISE_MAX;
  pad->wakeAbs = fed4TouchWakeThresholdForPad(pad->idleMean, pad->riseThresh);
}

bool fed4TouchCalValid(const Fed4TouchCal *cal)
{
  if (!cal || cal->ver != FED4_TOUCH_CAL_VER)
    return false;
  if (cal->mapL != (uint8_t)TOUCH_PAD_LEFT ||
      cal->mapC != (uint8_t)TOUCH_PAD_CENTER ||
      cal->mapR != (uint8_t)TOUCH_PAD_RIGHT)
    return false;
  if (!cal->L.idleMean || !cal->C.idleMean || !cal->R.idleMean)
    return false;
  if (!cal->L.touchDelta || !cal->C.touchDelta || !cal->R.touchDelta)
    return false;
  if (!cal->L.wakeAbs || !cal->C.wakeAbs || !cal->R.wakeAbs)
    return false;
  return true;
}

bool fed4TouchCalSave(const Fed4TouchCal *cal)
{
  if (!fed4TouchCalValid(cal))
    return false;

  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false))
    return false;

  const uint32_t mapPacked =
      ((uint32_t)cal->mapL) | ((uint32_t)cal->mapC << 8) | ((uint32_t)cal->mapR << 16);
  const bool ok = prefs.putUChar(kTouchCalVerKey, cal->ver) > 0 &&
                  prefs.putUInt(kTouchCalMapKey, mapPacked) > 0 &&
                  prefs.putBytes(kTouchCalBlobKey, cal, sizeof(Fed4TouchCal)) ==
                      sizeof(Fed4TouchCal);
  prefs.end();
  if (ok)
    Serial.println("Touch cal: saved to NVS");
  return ok;
}

bool fed4TouchCalLoad(Fed4TouchCal *out)
{
  if (!out)
    return false;

  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, true))
    return false;

  const uint8_t ver = prefs.getUChar(kTouchCalVerKey, 0);
  Fed4TouchCal cal = {};
  const size_t n = prefs.getBytes(kTouchCalBlobKey, &cal, sizeof(Fed4TouchCal));
  prefs.end();

  if (ver != FED4_TOUCH_CAL_VER || n != sizeof(Fed4TouchCal))
    return false;
  if (!fed4TouchCalValid(&cal))
    return false;

  *out = cal;
  return true;
}

bool fed4TouchCalClear(void)
{
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false))
    return false;
  prefs.remove(kTouchCalVerKey);
  prefs.remove(kTouchCalMapKey);
  prefs.remove(kTouchCalBlobKey);
  prefs.end();
  Serial.println("Touch cal: cleared from NVS");
  return true;
}

bool fed4TouchCalApply(const Fed4TouchCal *cal)
{
  if (!fed4TouchCalValid(cal))
    return false;

  fed4TouchIdleL = cal->L.idleMean;
  fed4TouchIdleC = cal->C.idleMean;
  fed4TouchIdleR = cal->R.idleMean;
  fed4TouchStdL = cal->L.idleStd;
  fed4TouchStdC = cal->C.idleStd;
  fed4TouchStdR = cal->R.idleStd;
  fed4TouchRiseThreshL = cal->L.riseThresh;
  fed4TouchRiseThreshC = cal->C.riseThresh;
  fed4TouchRiseThreshR = cal->R.riseThresh;

  fed4TouchCalPrint(cal);
  return fed4TouchApplyThresholds(cal->L.wakeAbs, cal->C.wakeAbs, cal->R.wakeAbs);
}

void fed4TouchCalPrint(const Fed4TouchCal *cal)
{
  if (!cal)
    return;
  Serial.printf("Touch cal v%u map L/C/R=%u/%u/%u unix=%lu\n", cal->ver, cal->mapL,
                cal->mapC, cal->mapR, (unsigned long)cal->unixTime);
  Serial.printf("  L idle=%lu std=%.1f delta=%lu wakeAbs=%lu rise=%.4f\n",
                (unsigned long)cal->L.idleMean, (double)cal->L.idleStd,
                (unsigned long)cal->L.touchDelta, (unsigned long)cal->L.wakeAbs,
                (double)cal->L.riseThresh);
  Serial.printf("  C idle=%lu std=%.1f delta=%lu wakeAbs=%lu rise=%.4f\n",
                (unsigned long)cal->C.idleMean, (double)cal->C.idleStd,
                (unsigned long)cal->C.touchDelta, (unsigned long)cal->C.wakeAbs,
                (double)cal->C.riseThresh);
  Serial.printf("  R idle=%lu std=%.1f delta=%lu wakeAbs=%lu rise=%.4f\n",
                (unsigned long)cal->R.idleMean, (double)cal->R.idleStd,
                (unsigned long)cal->R.touchDelta, (unsigned long)cal->R.wakeAbs,
                (double)cal->R.riseThresh);
  Serial.flush();
}

bool FED4::touchCalSave(const Fed4TouchCal &cal)
{
  return fed4TouchCalSave(&cal);
}

bool FED4::touchCalLoad(Fed4TouchCal *out)
{
  return fed4TouchCalLoad(out);
}

bool FED4::touchCalClear()
{
  return fed4TouchCalClear();
}

bool FED4::touchCalApply(const Fed4TouchCal &cal)
{
  return fed4TouchCalApply(&cal);
}

float fed4TouchRiseFraction(uint32_t raw, uint32_t idle)
{
  if (!idle || raw <= idle)
    return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

uint32_t fed4TouchWakeThreshold(uint32_t idle)
{
  // active_thresh is a delta above benchmark (legacy helper uses floor).
  return fed4TouchWakeThresholdForPad(idle, TOUCH_THRESHOLD);
}

uint32_t fed4TouchWakeThresholdForPad(uint32_t idle, float riseThresh)
{
  return (uint32_t)(idle * riseThresh);
}

uint32_t fed4TouchRead(uint8_t pin)
{
  return fed4TouchReadChannel(pin, false);
}

uint32_t fed4TouchReadBenchmark(uint8_t pin)
{
  return fed4TouchReadChannel(pin, true);
}

int fed4TouchLastLatchPad(void) { return sTouchLastLatchPad; }

int fed4TouchLastConfirmPad(void) { return sTouchLastConfirmPad; }

uint32_t fed4TouchLastPeakSmooth(void) { return sTouchLastPeakSmooth; }

uint32_t fed4TouchRecharCount(void) { return sTouchRecharCount; }

void fed4TouchNoteRechar(void) { sTouchRecharCount++; }

void fed4TouchSetLastPoke(int latchPad, int confirmPad, uint32_t peakSmooth)
{
  sTouchLastLatchPad = latchPad;
  sTouchLastConfirmPad = confirmPad;
  sTouchLastPeakSmooth = peakSmooth;
}

bool fed4TouchPadsReleased(float riseLimit)
{
  // Prefer characterized per-pad thresholds. riseLimit is only an override when
  // it is *lower* (stricter release / more sensitive detect) — never a floor,
  // or high-baseline pads (Left) stay stuck behind a large % of idle.
  const float thrL =
      (riseLimit > 0.0f && riseLimit < fed4TouchRiseThreshL) ? riseLimit
                                                             : fed4TouchRiseThreshL;
  const float thrC =
      (riseLimit > 0.0f && riseLimit < fed4TouchRiseThreshC) ? riseLimit
                                                             : fed4TouchRiseThreshC;
  const float thrR =
      (riseLimit > 0.0f && riseLimit < fed4TouchRiseThreshR) ? riseLimit
                                                             : fed4TouchRiseThreshR;

  const float fl = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_LEFT), fed4TouchIdleL);
  const float fc = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_CENTER), fed4TouchIdleC);
  const float fr = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_RIGHT), fed4TouchIdleR);
  return fl < thrL && fc < thrC && fr < thrR;
}

bool fed4TouchAnyPadActive(float riseLimit)
{
  return !fed4TouchPadsReleased(riseLimit);
}

bool fed4TouchAllPadsHwInactive(void)
{
  const uint8_t pins[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t wakeAbs[3] = {fed4TouchWakeAbsL, fed4TouchWakeAbsC, fed4TouchWakeAbsR};
  for (int i = 0; i < 3; i++)
  {
    if (!wakeAbs[i])
      continue; // not yet characterized — don't block on an unknown pad
    const uint32_t sm = fed4TouchRead(pins[i]);
    const uint32_t bm = fed4TouchReadBenchmark(pins[i]);
    if ((int32_t)sm - (int32_t)bm >= (int32_t)wakeAbs[i])
      return false;
  }
  return true;
}

bool fed4TouchBenchWatchdog(void)
{
  const uint8_t pins[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  uint32_t *sinceMs[3] = {&sBenchStuckSinceMsL, &sBenchStuckSinceMsC,
                          &sBenchStuckSinceMsR};
  const uint32_t wakeAbs[3] = {fed4TouchWakeAbsL, fed4TouchWakeAbsC, fed4TouchWakeAbsR};
  const uint32_t nowMs = millis();
  bool fired = false;

  for (int i = 0; i < 3; i++)
  {
    if (!wakeAbs[i])
    {
      *sinceMs[i] = 0;
      continue;
    }
    const uint32_t sm = fed4TouchRead(pins[i]);
    const uint32_t bm = fed4TouchReadBenchmark(pins[i]);
    const int32_t d = (int32_t)sm - (int32_t)bm;
    if (d >= (int32_t)wakeAbs[i])
    {
      if (!*sinceMs[i])
      {
        *sinceMs[i] = nowMs;
      }
      else if ((nowMs - *sinceMs[i]) >= FED4_TOUCH_BENCH_STUCK_MS)
      {
        touch_pad_reset_benchmark((touch_pad_t)pins[i]);
        Serial.printf(
            "Touch: pad %d benchmark stuck %lu ms — reset (smooth=%lu bench=%lu wakeAbs=%lu)\n",
            pins[i], (unsigned long)FED4_TOUCH_BENCH_STUCK_MS, (unsigned long)sm,
            (unsigned long)bm, (unsigned long)wakeAbs[i]);
        *sinceMs[i] = 0;
        fired = true;
      }
    }
    else
    {
      *sinceMs[i] = 0;
    }
  }
  return fired;
}

uint32_t fed4TouchBenchStuckMs(int padIndex)
{
  uint32_t sinceMs;
  switch (padIndex)
  {
  case 1:
    sinceMs = sBenchStuckSinceMsL;
    break;
  case 2:
    sinceMs = sBenchStuckSinceMsC;
    break;
  case 3:
    sinceMs = sBenchStuckSinceMsR;
    break;
  default:
    return 0;
  }
  return sinceMs ? (millis() - sinceMs) : 0;
}

bool fed4TouchInitPads(void)
{
  if (!sTouchInited)
  {
    if (!fed4TouchCreateController())
      return false;
    sTouchInited = true;
  }
  return fed4TouchCharacterizePads();
}

bool fed4TouchEnableTouchpadWakeup(void)
{
  return esp_sleep_enable_touchpad_wakeup() == ESP_OK;
}

void fed4TouchClearWakePadLatch(void)
{
  fed4TouchClearActiveLatch();
}

/**
 * Resolve which pad the hardware considers active.
 * Light sleep: esp_sleep_get_touchpad_wakeup_status() is deep-sleep-only — ignore it.
 * Prefer on_active latch, then (smooth − benchmark) vs configured wakeAbs.
 */
int fed4TouchPadIndexFromHwWakeStatus(void)
{
  // 1) Driver latch (set when channel goes active — including after sleep wake)
  int pad = fed4TouchPadIndexFromLatchOnly();
  if (pad)
    return pad;

  // 2) Same model as HW wake: smooth − benchmark ≥ active_thresh
  const uint8_t pins[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t absThr[3] = {fed4TouchWakeAbsL, fed4TouchWakeAbsC,
                              fed4TouchWakeAbsR};
  int32_t bestDelta = -1;
  int bestPad = 0;
  int bestChan = -1;
  for (int i = 0; i < 3; i++)
  {
    if (!absThr[i])
      continue;
    const uint32_t sm = fed4TouchRead(pins[i]);
    const uint32_t bm = fed4TouchReadBenchmark(pins[i]);
    const int32_t d = (int32_t)sm - (int32_t)bm;
    if (d >= (int32_t)absThr[i] && d > bestDelta)
    {
      bestDelta = d;
      bestPad = i + 1;
      bestChan = pins[i];
    }
  }
  if (bestPad)
    sTouchLastResolvedChan = bestChan;
  return bestPad;
}

/**
 * Newly-active status_mask edge (accumulated since the last clear — see
 * fed4TouchOnActive()). If exactly one of our three channels edged active,
 * that is unambiguous and is returned directly. touch_pad_get_current_meas_channel()
 * is deliberately NOT consulted here: it reports whichever channel the FSM
 * happens to be scanning at ISR-entry time, not the channel that went active,
 * and against ms-scale measurements it is almost always the *next* channel in
 * scan order — a one-slot skew that silently remapped Left→Right→Center in
 * the field (see docs/wiki/Poke-Functionality.md). If the edge is ambiguous
 * (zero or more than one bit — e.g. two pads edged between reads), fall back
 * to the strongest live smooth−benchmark delta among the set bits.
 */
int fed4TouchPadIndexFromLatchOnly(void)
{
  const uint32_t mask = sTouchActiveMask;
  if (!mask)
    return 0;

  const int chans[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  int bitsSet = 0, onlyPad = 0, onlyChan = -1;
  for (int i = 0; i < 3; i++)
  {
    if (mask & (1u << chans[i]))
    {
      bitsSet++;
      onlyPad = i + 1;
      onlyChan = chans[i];
    }
  }

  if (bitsSet == 1)
  {
    sTouchLastResolvedChan = onlyChan;
    return onlyPad;
  }
  if (bitsSet == 0)
    return 0;

  // Ambiguous — more than one channel edged active between reads.
  int32_t bestDelta = -1;
  int bestPad = 0;
  int bestChan = -1;
  for (int i = 0; i < 3; i++)
  {
    if (!(mask & (1u << chans[i])))
      continue;
    const uint32_t sm = fed4TouchRead(chans[i]);
    const uint32_t bm = fed4TouchReadBenchmark(chans[i]);
    const int32_t d = (int32_t)sm - (int32_t)bm;
    if (d > bestDelta)
    {
      bestDelta = d;
      bestPad = i + 1;
      bestChan = chans[i];
    }
  }
  if (bestPad)
    sTouchLastResolvedChan = bestChan;
  return bestPad;
}

/**
 * Confirm pad by absolute (smooth − idle) over a short window.
 * Avoids first-cross rise% false hits on high-baseline Left after sleep.
 * Never returns an unvoted single-sample winner (F5): with the pre-fix ~22 ms
 * scan and 5 ms sampling, this used to be 6 reads of one hardware value, so a
 * run of TOUCH_ID_CONFIRM_AGREE "agreeing" samples was trivially satisfied by
 * one noisy reading — it decided 34% of pokes wrong in the field (see
 * docs/wiki/Poke-Functionality.md). If no genuine run of agreeing samples is
 * seen, abstain (return 0) and let the caller trust the (now-fixed) latch.
 */
int fed4TouchConfirmPadByAbsDelta(void)
{
  const uint8_t pins[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t idles[3] = {fed4TouchIdleL, fed4TouchIdleC, fed4TouchIdleR};
  const uint32_t floors[3] = {
      fed4TouchWakeAbsL ? fed4TouchWakeAbsL : (uint32_t)TOUCH_CHAR_ABS_MIN,
      fed4TouchWakeAbsC ? fed4TouchWakeAbsC : (uint32_t)TOUCH_CHAR_ABS_MIN,
      fed4TouchWakeAbsR ? fed4TouchWakeAbsR : (uint32_t)TOUCH_CHAR_ABS_MIN};

  int agreePad = 0;
  int agreeCount = 0;

  const int maxSamples =
      (TOUCH_ID_CONFIRM_DT_MS > 0)
          ? (TOUCH_ID_CONFIRM_MS / TOUCH_ID_CONFIRM_DT_MS)
          : 1;

  for (int s = 0; s < maxSamples; s++)
  {
    if (s)
      delay(TOUCH_ID_CONFIRM_DT_MS);

    int32_t d[3] = {0, 0, 0};
    int32_t bestD = -1;
    int32_t secondD = -1;
    int bestI = -1;

    for (int i = 0; i < 3; i++)
    {
      if (!idles[i])
        continue;
      const uint32_t sm = fed4TouchRead(pins[i]);
      int32_t di = (int32_t)sm - (int32_t)idles[i];
      if (di < 0)
        di = 0;
      d[i] = di;
      if (di >= (int32_t)floors[i])
      {
        if (di > bestD)
        {
          secondD = bestD;
          bestD = di;
          bestI = i;
        }
        else if (di > secondD)
        {
          secondD = di;
        }
      }
    }

    sTouchLastConfirmDeltaL = d[0];
    sTouchLastConfirmDeltaC = d[1];
    sTouchLastConfirmDeltaR = d[2];

    int samplePad = 0;
    if (bestI >= 0)
    {
      const bool marginOk =
          (TOUCH_ID_CONFIRM_MARGIN <= 0.0f) || (secondD < 0) ||
          ((float)bestD >= (float)secondD * (1.0f + TOUCH_ID_CONFIRM_MARGIN));
      if (marginOk)
        samplePad = bestI + 1;
    }

    if (samplePad)
    {
      if (samplePad == agreePad)
        agreeCount++;
      else
      {
        agreePad = samplePad;
        agreeCount = 1;
      }
      if (agreeCount >= TOUCH_ID_CONFIRM_AGREE)
      {
        const int chans[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
        sTouchLastResolvedChan = chans[agreePad - 1];
        sTouchLastConfirmAgreed = true;
        return agreePad;
      }
    }
    else
    {
      agreePad = 0;
      agreeCount = 0;
    }
  }

  sTouchLastConfirmAgreed = false;
  return 0;
}

int fed4TouchIdentifyWakePadIndex(float triggerRise)
{
  const uint32_t l = fed4TouchRead(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchRead(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchRead(TOUCH_PAD_RIGHT);

  const float fl = fed4TouchRiseFraction(l, fed4TouchIdleL);
  const float fc = fed4TouchRiseFraction(c, fed4TouchIdleC);
  const float fr = fed4TouchRiseFraction(r, fed4TouchIdleR);

  const float thrL =
      (triggerRise > 0.0f && triggerRise < fed4TouchRiseThreshL) ? triggerRise
                                                                 : fed4TouchRiseThreshL;
  const float thrC =
      (triggerRise > 0.0f && triggerRise < fed4TouchRiseThreshC) ? triggerRise
                                                                 : fed4TouchRiseThreshC;
  const float thrR =
      (triggerRise > 0.0f && triggerRise < fed4TouchRiseThreshR) ? triggerRise
                                                                 : fed4TouchRiseThreshR;

  const bool aL = fl >= thrL;
  const bool aC = fc >= thrC;
  const bool aR = fr >= thrR;
  if (!aL && !aC && !aR)
    return 0;

  if (aL && fl >= fc && fl >= fr)
    return 1;
  if (aC && fc >= fr)
    return 2;
  if (aR)
    return 3;
  // Fallback: strongest among those that cleared their thresh
  if (aL)
    return 1;
  if (aC)
    return 2;
  return 3;
}

const char *fed4TouchIdentifyWakePad(float triggerRise)
{
  switch (fed4TouchIdentifyWakePadIndex(triggerRise))
  {
  case 1:
    return "LEFT";
  case 2:
    return "CENTER";
  case 3:
    return "RIGHT";
  default:
    return nullptr;
  }
}

void fed4TouchPrintDriverConfig(void)
{
  Serial.println("Touch: legacy touch_pad + BM IIR8 + denoise BIT4/L4 + smooth=raw + debounce1");
  Serial.printf(
      "Touch: char warm=%ums n=%d dt=%ums sigma=%.1f absMin=%.0f absMargin=%.0f\n",
      (unsigned)TOUCH_CHAR_WARM_MS, TOUCH_CHAR_SAMPLES,
      (unsigned)TOUCH_CHAR_INTERVAL_MS, (double)TOUCH_CHAR_SIGMA,
      (double)TOUCH_CHAR_ABS_MIN, (double)TOUCH_CHAR_ABS_MARGIN);
  Serial.printf(
      "Touch: measureCycles=%u scanPeriodUs=%lu measUs L=%lu C=%lu R=%lu\n",
      (unsigned)TOUCH_MEASURE_CYCLES, (unsigned long)sScanPeriodUs,
      (unsigned long)sMeasUsL, (unsigned long)sMeasUsC, (unsigned long)sMeasUsR);
}

#if FED4_DIAG_POKE_TIMING
static uint32_t sPokeT0Us = 0;
static uint32_t sPokeMarkUs[FED4_POKE_T_COUNT];
static uint8_t sPokeMarkSet = 0;

void fed4PokeTimingReset(void)
{
  sPokeT0Us = micros();
  sPokeMarkSet = 0;
  for (int i = 0; i < FED4_POKE_T_COUNT; i++)
    sPokeMarkUs[i] = 0;
}

void fed4PokeTimingMark(int id)
{
  if (id < 0 || id >= FED4_POKE_T_COUNT)
    return;
  sPokeMarkUs[id] = micros() - sPokeT0Us;
  sPokeMarkSet |= (uint8_t)(1u << id);
}

void fed4PokeTimingPrint(const char *note)
{
  auto us = [](int id) -> unsigned long {
    return (unsigned long)sPokeMarkUs[id];
  };
  Serial.printf(
      "POKE_TIMING %s us: wake=%lu wakeUp=%lu preCap=%lu id=%lu capDone=%lu "
      "class=%lu log=%lu update=%lu | gpioChan=%d mask=0x%lx\n",
      note ? note : "",
      us(FED4_POKE_T_WAKE), us(FED4_POKE_T_WAKEUP), us(FED4_POKE_T_PRE_CAPTURE),
      us(FED4_POKE_T_IDENTIFIED), us(FED4_POKE_T_CAPTURE_DONE),
      us(FED4_POKE_T_CLASSIFIED), us(FED4_POKE_T_LOG_DONE),
      us(FED4_POKE_T_UPDATE_DONE),
      sTouchLastResolvedChan, (unsigned long)sTouchActiveMask);
  Serial.flush();
  (void)sPokeMarkSet;
}
#else
void fed4PokeTimingReset(void) {}
void fed4PokeTimingMark(int id) { (void)id; }
void fed4PokeTimingPrint(const char *note) { (void)note; }
#endif

// ---------------------------------------------------------------------------
// FED4 class API
// ---------------------------------------------------------------------------

bool FED4::initializeTouch()
{
  if (!fed4TouchInitPads())
    return false;
  fed4TouchEnableTouchpadWakeup();
  return true;
}

void FED4::calibrateTouchSensors(bool checkStability)
{
  wakePad = 0;

  if (checkStability && fed4TouchIdleL && fed4TouchIdleC && fed4TouchIdleR)
  {
    const int maxReleaseAttempts = 40;
    int attempts = 0;
    while (attempts < maxReleaseAttempts && !fed4TouchPadsReleased(TOUCH_THRESHOLD))
    {
      attempts++;
      delay(5);
    }
  }

  if (!fed4TouchCharacterizePads())
  {
    Serial.println("Touch: characterization FAILED");
  }
  fed4TouchEnableTouchpadWakeup();
  wakePad = 0;
}

/**
 * Resolve poke pad + measure hold time (pokeDuration).
 * Latch (who woke sleep) + absolute-(smooth−idle) confirm over ~30 ms.
 * Agree → latch; disagree → confirm; confirm only / latch only as fallbacks.
 */
bool FED4::capturePoke()
{
  resetTouchFlags();
  wakePad = 0;
  pokeDuration = 0.0f;

  const int latchPad = fed4TouchPadIndexFromLatchOnly();
  const int confirmPad = fed4TouchConfirmPadByAbsDelta();
  const bool confirmAgreed = fed4TouchLastConfirmAgreed();
  (void)confirmAgreed; // only read inside FED4_DIAG_POKE_TIMING serial prints below

  // Snapshot the identification decision for the touch drift log
  sTouchLastLatchPad = latchPad;
  sTouchLastConfirmPad = confirmPad;
  sTouchLastPeakSmooth = 0;
  sTouchLastPeakL = 0;
  sTouchLastPeakC = 0;
  sTouchLastPeakR = 0;

#if FED4_DIAG_POKE_TIMING
  Serial.printf("TOUCHISR chan=%d isrMask=0x%lx liveStatus=0x%lx n=%lu\n",
                fed4TouchLastIsrChan(), (unsigned long)fed4TouchLastIsrMask(),
                (unsigned long)fed4TouchLiveStatusMask(),
                (unsigned long)fed4TouchIsrCount());
#endif

  int padIndex = 0;
  if (latchPad && confirmPad)
  {
    // confirmPad is only ever nonzero when fed4TouchConfirmPadByAbsDelta()
    // actually voted it (F5) — so preferring it over a disagreeing latch here
    // is no longer "trusting a single noisy sample" the way it used to be.
    if (latchPad == confirmPad)
      padIndex = latchPad;
    else
    {
      padIndex = confirmPad;
#if FED4_DIAG_POKE_TIMING
      Serial.printf(
          "Touch ID: latch=%d confirm=%d agreed=%d (using confirm) dL=%ld dC=%ld dR=%ld\n",
          latchPad, confirmPad, (int)confirmAgreed,
          (long)fed4TouchLastConfirmDelta(1), (long)fed4TouchLastConfirmDelta(2),
          (long)fed4TouchLastConfirmDelta(3));
#endif
    }
  }
  else if (confirmPad)
  {
    padIndex = confirmPad;
  }
  else if (latchPad)
  {
    padIndex = latchPad; // quick tap: finger gone, or confirm abstained — trust the (now-fixed) latch
#if FED4_DIAG_POKE_TIMING
    Serial.printf(
        "Touch ID: latch=%d confirm=0 agreed=%d (using latch) dL=%ld dC=%ld dR=%ld\n",
        latchPad, (int)confirmAgreed, (long)fed4TouchLastConfirmDelta(1),
        (long)fed4TouchLastConfirmDelta(2), (long)fed4TouchLastConfirmDelta(3));
#endif
  }

  fed4TouchClearActiveLatch();
  if (padIndex == 0)
    return false;

  fed4PokeTimingMark(FED4_POKE_T_IDENTIFIED);

  const unsigned long touchStartTime = millis();
  const unsigned long maxSamplingTime_ms = 500;
  const int minReleaseReadings = 2;
  int belowThresholdCount = 0;

  // Poke amplitude on the resolved pad — the sensitivity metric (baselines alone
  // cannot separate drift from coupling loss). Also tracks all three pads'
  // peaks (PeakL/C/R) as an offline ground truth independent of latch/confirm.
  const uint8_t peakPin = (padIndex == 1)   ? TOUCH_PAD_LEFT
                          : (padIndex == 2) ? TOUCH_PAD_CENTER
                                            : TOUCH_PAD_RIGHT;

  while (millis() - touchStartTime < maxSamplingTime_ms)
  {
    const uint32_t sL = fed4TouchRead(TOUCH_PAD_LEFT);
    const uint32_t sC = fed4TouchRead(TOUCH_PAD_CENTER);
    const uint32_t sR = fed4TouchRead(TOUCH_PAD_RIGHT);
    if (sL > sTouchLastPeakL)
      sTouchLastPeakL = sL;
    if (sC > sTouchLastPeakC)
      sTouchLastPeakC = sC;
    if (sR > sTouchLastPeakR)
      sTouchLastPeakR = sR;

    const uint32_t peakSample = (peakPin == TOUCH_PAD_LEFT)   ? sL
                                : (peakPin == TOUCH_PAD_CENTER) ? sC
                                                                : sR;
    if (peakSample > sTouchLastPeakSmooth)
      sTouchLastPeakSmooth = peakSample;

    if (fed4TouchPadsReleased(TOUCH_THRESHOLD))
    {
      belowThresholdCount++;
      if (belowThresholdCount >= minReleaseReadings)
        break;
    }
    else
    {
      belowThresholdCount = 0;
    }
    delay(1);
  }

  pokeDuration = (float)(millis() - touchStartTime);

  const FedPad pad = static_cast<FedPad>(padIndex);
  wakePad = (uint8_t)padIndex;

  if (pad == FedPad::Left)
  {
    leftCount++;
    leftTouch = true;
  }
  else if (pad == FedPad::Center)
  {
    centerCount++;
    centerTouch = true;
  }
  else if (pad == FedPad::Right)
  {
    rightCount++;
    rightTouch = true;
  }

  return true;
}

void FED4::resetTouchFlags()
{
  leftTouch = false;
  centerTouch = false;
  rightTouch = false;
}
