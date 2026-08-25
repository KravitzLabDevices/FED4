#include "FED4.h"
#include "FED4_TouchHelpers.h"

#include <math.h>
#include "driver/touch_sens.h"

// ---------------------------------------------------------------------------
// NG touch_sens driver (latest IDF only — no Arduino touchRead / legacy path)
// ---------------------------------------------------------------------------

static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
static const uint16_t TOUCH_SLEEP_CYCLES = 500;

uint32_t fed4TouchIdleL = 0;
uint32_t fed4TouchIdleC = 0;
uint32_t fed4TouchIdleR = 0;

float fed4TouchStdL = 0.0f;
float fed4TouchStdC = 0.0f;
float fed4TouchStdR = 0.0f;

float fed4TouchRiseThreshL = TOUCH_THRESHOLD;
float fed4TouchRiseThreshC = TOUCH_THRESHOLD;
float fed4TouchRiseThreshR = TOUCH_THRESHOLD;

uint8_t FED4::wakePad = 0; // 0=none, 1=left, 2=center, 3=right (sync of FedPad; not an ISR latch)

static touch_sensor_handle_t sTouchSens = NULL;
static touch_channel_handle_t sTouchChanLeft = NULL;
static touch_channel_handle_t sTouchChanCenter = NULL;
static touch_channel_handle_t sTouchChanRight = NULL;
static touch_channel_handle_t sTouchChanByPad[TOUCH_PAD_MAX] = {};

static touch_sensor_filter_config_t fed4TouchFilterConfig()
{
  touch_sensor_filter_config_t cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
  cfg.benchmark.filter_mode = TOUCH_BM_IIR_FILTER_16;
  cfg.benchmark.denoise_lvl = 4;
  cfg.data.debounce_cnt = 1;
  return cfg;
}

static bool fed4TouchNgStopDisable()
{
  touch_sensor_stop_continuous_scanning(sTouchSens);
  return touch_sensor_disable(sTouchSens) == ESP_OK;
}

static bool fed4TouchNgEnableStart()
{
  if (touch_sensor_enable(sTouchSens) != ESP_OK)
    return false;
  return touch_sensor_start_continuous_scanning(sTouchSens) == ESP_OK;
}

static bool fed4TouchNgAddChannel(int chanId, touch_channel_handle_t *outHandle)
{
  touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
  if (touch_sensor_new_channel(sTouchSens, chanId, &chan_cfg, outHandle) != ESP_OK)
    return false;
  sTouchChanByPad[chanId] = *outHandle;
  return true;
}

static bool fed4TouchNgCreateController()
{
  touch_sensor_sample_config_t sample_cfg = TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
      TOUCH_MEASURE_CYCLES, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V7);
  touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, &sample_cfg);
  sens_cfg.power_on_wait_us = TOUCH_SLEEP_CYCLES;
  sens_cfg.meas_interval_us = 32.0f;

  if (touch_sensor_new_controller(&sens_cfg, &sTouchSens) != ESP_OK)
    return false;

  if (!fed4TouchNgAddChannel(TOUCH_PAD_LEFT, &sTouchChanLeft))
    return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_CENTER, &sTouchChanCenter))
    return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_RIGHT, &sTouchChanRight))
    return false;

#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_LSLP_CONFIG();
  if (touch_sensor_config_sleep_wakeup(sTouchSens, &sleep_cfg) != ESP_OK)
    return false;
#endif

  if (!fed4TouchNgEnableStart())
    return false;

  touch_sensor_filter_config_t filter_cfg = fed4TouchFilterConfig();
  return touch_sensor_config_filter(sTouchSens, &filter_cfg) == ESP_OK;
}

static bool fed4TouchNgApplyThresholds(uint32_t threshL, uint32_t threshC,
                                       uint32_t threshR)
{
  const int pads[] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t thresh[] = {threshL, threshC, threshR};

  if (!fed4TouchNgStopDisable())
    return false;

  for (int i = 0; i < 3; i++)
  {
    touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
    chan_cfg.active_thresh[0] = thresh[i];
    if (touch_sensor_reconfig_channel(sTouchChanByPad[pads[i]], &chan_cfg) != ESP_OK)
      return false;
  }

#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_LSLP_CONFIG();
  if (touch_sensor_config_sleep_wakeup(sTouchSens, &sleep_cfg) != ESP_OK)
    return false;
#endif

  return fed4TouchNgEnableStart();
}

static uint32_t fed4TouchNgReadChannel(uint8_t pin, touch_chan_data_type_t type)
{
  const int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0 || pad >= TOUCH_PAD_MAX || !sTouchChanByPad[pad])
    return 0;

  uint32_t value[TOUCH_SAMPLE_CFG_NUM] = {};
  if (touch_channel_read_data(sTouchChanByPad[pad], type, value) != ESP_OK)
    return 0;
  return value[0];
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
    samplesL[i] = fed4TouchNgReadChannel(TOUCH_PAD_LEFT, TOUCH_CHAN_DATA_TYPE_SMOOTH);
    samplesC[i] = fed4TouchNgReadChannel(TOUCH_PAD_CENTER, TOUCH_CHAN_DATA_TYPE_SMOOTH);
    samplesR[i] = fed4TouchNgReadChannel(TOUCH_PAD_RIGHT, TOUCH_CHAN_DATA_TYPE_SMOOTH);
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
    // across pads with very different baselines (Left ~220k vs C/R ~130–140k).
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
    const uint32_t raw = fed4TouchNgReadChannel(pads[i], TOUCH_CHAN_DATA_TYPE_SMOOTH);
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

  return fed4TouchNgApplyThresholds(
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

float fed4TouchRiseFraction(uint32_t raw, uint32_t idle)
{
  if (!idle || raw <= idle)
    return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

uint32_t fed4TouchWakeThreshold(uint32_t idle)
{
  // NG active_thresh is a delta above benchmark (legacy helper uses floor).
  return fed4TouchWakeThresholdForPad(idle, TOUCH_THRESHOLD);
}

uint32_t fed4TouchWakeThresholdForPad(uint32_t idle, float riseThresh)
{
  return (uint32_t)(idle * riseThresh);
}

uint32_t fed4TouchRead(uint8_t pin)
{
  return fed4TouchNgReadChannel(pin, TOUCH_CHAN_DATA_TYPE_SMOOTH);
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
  if (sTouchSens == NULL)
  {
    if (!fed4TouchNgCreateController())
      return false;
  }
  return fed4TouchCharacterizePads();
}

bool fed4TouchEnableTouchpadWakeup(void)
{
  return esp_sleep_enable_touchpad_wakeup() == ESP_OK;
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
  Serial.println("Touch: NG driver + BM IIR16 + denoise4 + debounce1");
  Serial.printf(
      "Touch: char warm=%ums n=%d dt=%ums sigma=%.1f absMin=%.0f absMargin=%.0f\n",
      (unsigned)TOUCH_CHAR_WARM_MS, TOUCH_CHAR_SAMPLES,
      (unsigned)TOUCH_CHAR_INTERVAL_MS, (double)TOUCH_CHAR_SIGMA,
      (double)TOUCH_CHAR_ABS_MIN, (double)TOUCH_CHAR_ABS_MARGIN);
}

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
 * Identify an active poke via rise fraction (same model as unit tests).
 * After light sleep the NG smooth filter needs a few samples before rise is valid.
 */
bool FED4::capturePoke()
{
  resetTouchFlags();
  wakePad = 0;
  pokeDuration = 0.0f;

  int padIndex = 0;
  for (int attempt = 0; attempt < 25 && padIndex == 0; attempt++)
  {
    delay(2);
    padIndex = fed4TouchIdentifyWakePadIndex(TOUCH_THRESHOLD);
  }
  if (padIndex == 0)
    return false;

  const unsigned long touchStartTime = millis();
  const unsigned long maxSamplingTime_ms = 500;
  const int minReleaseReadings = 2;
  int belowThresholdCount = 0;

  while (millis() - touchStartTime < maxSamplingTime_ms)
  {
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
