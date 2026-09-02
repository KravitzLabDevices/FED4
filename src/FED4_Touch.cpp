#include "FED4.h"
#include "FED4_TouchHelpers.h"

#include <math.h>
#include "driver/touch_sensor.h"

// ---------------------------------------------------------------------------
// Legacy touch_pad driver (Arduino-ESP32 3.2.1 / IDF 5.4 — no NG touch_sens)
// ---------------------------------------------------------------------------

static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
// ~32 us between scans (NG meas_interval_us) at RTC_SLOW ~136 kHz. IDF default is 15.
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
// Latch the channel the ACTIVE ISR reports (ISR / post-wake).
static volatile int sTouchActiveChanId = -1;
static volatile uint32_t sTouchActiveMask = 0;
static int sTouchLastResolvedChan = -1; // for POKE_TIMING (survives latch clear)

// Diagnostic snapshots consumed by FED4::logTouch() (touch drift CSV).
static int sTouchLastLatchPad = 0;
static int sTouchLastConfirmPad = 0;
static uint32_t sTouchLastPeakSmooth = 0;
static uint32_t sTouchRecharCount = 0;

static int fed4TouchChanIdToPadIndex(int chanId)
{
  if (chanId == TOUCH_PAD_LEFT)
    return 1;
  if (chanId == TOUCH_PAD_CENTER)
    return 2;
  if (chanId == TOUCH_PAD_RIGHT)
    return 3;
  return 0;
}

// IDF rtc_isr dispatcher is assumed to clear RTCCNTL.int_st after dispatch
// (Arduino HAL never calls touch_pad_intr_clear either). If the ISR re-fires
// continuously on hardware, add touch_pad_intr_clear(TOUCH_PAD_INTR_MASK_ACTIVE).
static void IRAM_ATTR fed4TouchOnActive(void *)
{
  const uint32_t mask = touch_pad_read_intr_status_mask();
  if (!(mask & TOUCH_PAD_INTR_MASK_ACTIVE))
    return;
  sTouchActiveChanId = (int)touch_pad_get_current_meas_channel();
  sTouchActiveMask = touch_pad_get_status();
}

static void fed4TouchClearActiveLatch(void)
{
  sTouchActiveChanId = -1;
  sTouchActiveMask = 0;
}

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
  if (touch_pad_fsm_start() != ESP_OK)
    return false;

  touch_filter_config_t filter_cfg = fed4TouchFilterConfig();
  if (touch_pad_filter_set_config(&filter_cfg) != ESP_OK)
    return false;
  if (touch_pad_filter_enable() != ESP_OK)
    return false;

  // Do not call touch_pad_sleep_channel_enable() — that is the deep-sleep
  // single-channel path and would break multi-pad light-sleep wake.
  if (touch_pad_isr_register(fed4TouchOnActive, NULL, TOUCH_PAD_INTR_MASK_ACTIVE) !=
      ESP_OK)
    return false;
  return touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE) == ESP_OK;
}

static bool fed4TouchApplyThresholds(uint32_t threshL, uint32_t threshC,
                                     uint32_t threshR)
{
  const int pads[] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t thresh[] = {threshL, threshC, threshR};

  fed4TouchWakeAbsL = threshL;
  fed4TouchWakeAbsC = threshC;
  fed4TouchWakeAbsR = threshR;

  // Live set_thresh — do not stop/start the FSM. touch_pad_fsm_start()
  // resets the benchmark on all channels and would destroy drift tracking.
  for (int i = 0; i < 3; i++)
  {
    if (touch_pad_set_thresh((touch_pad_t)pads[i], thresh[i]) != ESP_OK)
      return false;
  }
  return true;
}

static uint32_t fed4TouchReadChannel(uint8_t pin, bool benchmark)
{
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

static bool fed4TouchAbsorbResidualOffset(void)
{
  // If a pad still looks "active" vs the new mean (settle after sampling),
  // lift idle to the current reading so release-wait cannot hang.
  const uint8_t pads[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  uint32_t *idles[3] = {&fed4TouchIdleL, &fed4TouchIdleC, &fed4TouchIdleR};
  float *threshs[3] = {&fed4TouchRiseThreshL, &fed4TouchRiseThreshC,
                       &fed4TouchRiseThreshR};
  bool adjusted = false;

  for (int i = 0; i < 3; i++)
  {
    const uint32_t raw = fed4TouchRead(pads[i]);
    const float rise = fed4TouchRiseFraction(raw, *idles[i]);
    if (rise >= *threshs[i] && raw > *idles[i])
    {
      Serial.printf("Touch: pad %d residual rise=%.3f — absorbing into idle (%lu → %lu)\n",
                    pads[i], (double)rise, (unsigned long)*idles[i],
                    (unsigned long)raw);
      *idles[i] = raw;
      adjusted = true;
    }
  }
  return adjusted;
}

/** Warm filter, characterize mean/std, set per-pad rise thresh + HW wake. */
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

  fed4TouchIdleL = statsL.mean;
  fed4TouchIdleC = statsC.mean;
  fed4TouchIdleR = statsR.mean;
  fed4TouchStdL = statsL.stddev;
  fed4TouchStdC = statsC.stddev;
  fed4TouchStdR = statsR.stddev;
  fed4TouchRiseThreshL = statsL.riseThresh;
  fed4TouchRiseThreshC = statsC.riseThresh;
  fed4TouchRiseThreshR = statsR.riseThresh;

  if (fed4TouchAbsorbResidualOffset())
  {
    (void)fed4TouchAbsorbResidualOffset();
  }

  fed4TouchPrintCharacterization();

  return fed4TouchApplyThresholds(
      fed4TouchWakeThresholdForPad(fed4TouchIdleL, fed4TouchRiseThreshL),
      fed4TouchWakeThresholdForPad(fed4TouchIdleC, fed4TouchRiseThreshC),
      fed4TouchWakeThresholdForPad(fed4TouchIdleR, fed4TouchRiseThreshR));
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

/** on_active chan_id, else strongest among status_mask bits (no live BM). */
int fed4TouchPadIndexFromLatchOnly(void)
{
  int pad = fed4TouchChanIdToPadIndex(sTouchActiveChanId);
  if (pad)
  {
    sTouchLastResolvedChan = sTouchActiveChanId;
    return pad;
  }

  if (!sTouchActiveMask)
    return 0;

  int32_t bestDelta = -1;
  int bestPad = 0;
  int bestChan = -1;
  const int chans[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  for (int i = 0; i < 3; i++)
  {
    if (!(sTouchActiveMask & (1u << chans[i])))
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
  int bestPadOverall = 0;
  int32_t bestDeltaOverall = -1;

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
      if (bestD > bestDeltaOverall)
      {
        bestDeltaOverall = bestD;
        bestPadOverall = samplePad;
      }
      if (agreeCount >= TOUCH_ID_CONFIRM_AGREE)
      {
        const int chans[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
        sTouchLastResolvedChan = chans[agreePad - 1];
        return agreePad;
      }
    }
    else
    {
      agreePad = 0;
      agreeCount = 0;
    }
  }

  if (bestPadOverall)
  {
    const int chans[3] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
    sTouchLastResolvedChan = chans[bestPadOverall - 1];
  }
  return bestPadOverall;
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

  // Snapshot the identification decision for the touch drift log
  sTouchLastLatchPad = latchPad;
  sTouchLastConfirmPad = confirmPad;
  sTouchLastPeakSmooth = 0;

  int padIndex = 0;
  if (latchPad && confirmPad)
  {
    if (latchPad == confirmPad)
      padIndex = latchPad;
    else
    {
      padIndex = confirmPad; // absolute Δ wins over stale/wrong latch
#if FED4_DIAG_POKE_TIMING
      Serial.printf("Touch ID: latch=%d confirm=%d (using confirm)\n", latchPad,
                    confirmPad);
#endif
    }
  }
  else if (confirmPad)
  {
    padIndex = confirmPad;
  }
  else if (latchPad)
  {
    padIndex = latchPad; // quick tap: finger gone, trust wake channel
#if FED4_DIAG_POKE_TIMING
    Serial.printf("Touch ID: latch=%d confirm=0 (using latch)\n", latchPad);
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
  // cannot separate drift from coupling loss).
  const uint8_t peakPin = (padIndex == 1)   ? TOUCH_PAD_LEFT
                          : (padIndex == 2) ? TOUCH_PAD_CENTER
                                            : TOUCH_PAD_RIGHT;

  while (millis() - touchStartTime < maxSamplingTime_ms)
  {
    const uint32_t peakSample = fed4TouchRead(peakPin);
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
