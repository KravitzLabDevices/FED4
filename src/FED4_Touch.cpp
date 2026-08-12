#include "FED4.h"
#include "FED4_TouchHelpers.h"

#include <cstring>
#include "driver/touch_sens.h"

// ---------------------------------------------------------------------------
// NG touch_sens driver (latest IDF only — no Arduino touchRead / legacy path)
// ---------------------------------------------------------------------------

static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
static const uint16_t TOUCH_SLEEP_CYCLES = 500;

uint32_t fed4TouchIdleL = 0;
uint32_t fed4TouchIdleC = 0;
uint32_t fed4TouchIdleR = 0;

uint8_t FED4::wakePad = 0;  // 0=none, 1=left, 2=center, 3=right

static touch_sensor_handle_t sTouchSens = NULL;
static touch_channel_handle_t sTouchChanLeft = NULL;
static touch_channel_handle_t sTouchChanCenter = NULL;
static touch_channel_handle_t sTouchChanRight = NULL;
static touch_channel_handle_t sTouchChanByPad[TOUCH_PAD_MAX] = {};

static touch_sensor_filter_config_t fed4TouchFilterConfig() {
  touch_sensor_filter_config_t cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
  cfg.benchmark.filter_mode = TOUCH_BM_IIR_FILTER_16;
  cfg.benchmark.denoise_lvl = 4;
  cfg.data.debounce_cnt = 1;
  return cfg;
}

static bool fed4TouchNgStopDisable() {
  touch_sensor_stop_continuous_scanning(sTouchSens);
  return touch_sensor_disable(sTouchSens) == ESP_OK;
}

static bool fed4TouchNgEnableStart() {
  if (touch_sensor_enable(sTouchSens) != ESP_OK) return false;
  return touch_sensor_start_continuous_scanning(sTouchSens) == ESP_OK;
}

static bool fed4TouchNgAddChannel(int chanId, touch_channel_handle_t *outHandle) {
  touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
  if (touch_sensor_new_channel(sTouchSens, chanId, &chan_cfg, outHandle) != ESP_OK)
    return false;
  sTouchChanByPad[chanId] = *outHandle;
  return true;
}

static bool fed4TouchNgCreateController() {
  touch_sensor_sample_config_t sample_cfg = TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
      TOUCH_MEASURE_CYCLES, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V7);
  touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, &sample_cfg);
  sens_cfg.power_on_wait_us = TOUCH_SLEEP_CYCLES;
  sens_cfg.meas_interval_us = 32.0f;

  if (touch_sensor_new_controller(&sens_cfg, &sTouchSens) != ESP_OK) return false;

  if (!fed4TouchNgAddChannel(TOUCH_PAD_LEFT, &sTouchChanLeft)) return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_CENTER, &sTouchChanCenter)) return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_RIGHT, &sTouchChanRight)) return false;

#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_LSLP_CONFIG();
  if (touch_sensor_config_sleep_wakeup(sTouchSens, &sleep_cfg) != ESP_OK) return false;
#endif

  if (!fed4TouchNgEnableStart()) return false;

  touch_sensor_filter_config_t filter_cfg = fed4TouchFilterConfig();
  return touch_sensor_config_filter(sTouchSens, &filter_cfg) == ESP_OK;
}

static bool fed4TouchNgApplyThresholds(uint32_t threshL, uint32_t threshC,
                                       uint32_t threshR) {
  const int pads[] = {TOUCH_PAD_LEFT, TOUCH_PAD_CENTER, TOUCH_PAD_RIGHT};
  const uint32_t thresh[] = {threshL, threshC, threshR};

  if (!fed4TouchNgStopDisable()) return false;

  for (int i = 0; i < 3; i++) {
    touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
    chan_cfg.active_thresh[0] = thresh[i];
    if (touch_sensor_reconfig_channel(sTouchChanByPad[pads[i]], &chan_cfg) != ESP_OK)
      return false;
  }

#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_LSLP_CONFIG();
  if (touch_sensor_config_sleep_wakeup(sTouchSens, &sleep_cfg) != ESP_OK) return false;
#endif

  return fed4TouchNgEnableStart();
}

static uint32_t fed4TouchNgReadChannel(uint8_t pin, touch_chan_data_type_t type) {
  const int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0 || pad >= TOUCH_PAD_MAX || !sTouchChanByPad[pad]) return 0;

  uint32_t value[TOUCH_SAMPLE_CFG_NUM] = {};
  if (touch_channel_read_data(sTouchChanByPad[pad], type, value) != ESP_OK)
    return 0;
  return value[0];
}

static uint32_t robustIdleAverage(uint8_t pad) {
  const int samples = 16;
  uint64_t sum = 0;
  uint32_t lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < samples; i++) {
    uint32_t v = fed4TouchNgReadChannel(pad, TOUCH_CHAN_DATA_TYPE_SMOOTH);
    sum += v;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
    delay(5);
  }
  return (uint32_t)((sum - lo - hi) / (samples - 2));
}

float fed4TouchRiseFraction(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

uint32_t fed4TouchWakeThreshold(uint32_t idle) {
  // NG active_thresh is a delta above benchmark
  return (uint32_t)(idle * TOUCH_THRESHOLD);
}

uint32_t fed4TouchRead(uint8_t pin) {
  return fed4TouchNgReadChannel(pin, TOUCH_CHAN_DATA_TYPE_SMOOTH);
}

uint32_t fed4TouchReadSmooth(uint8_t pin) {
  return fed4TouchNgReadChannel(pin, TOUCH_CHAN_DATA_TYPE_SMOOTH);
}

bool fed4TouchPadsReleased(float riseLimit) {
  const float fl = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_LEFT), fed4TouchIdleL);
  const float fc = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_CENTER), fed4TouchIdleC);
  const float fr = fed4TouchRiseFraction(fed4TouchRead(TOUCH_PAD_RIGHT), fed4TouchIdleR);
  return fl < riseLimit && fc < riseLimit && fr < riseLimit;
}

bool fed4TouchAnyPadActive(float riseLimit) {
  return !fed4TouchPadsReleased(riseLimit);
}

bool fed4TouchInitPads(void) {
  if (sTouchSens == NULL) {
    if (!fed4TouchNgCreateController()) return false;
  }

  for (int i = 0; i < 8; i++) {
    fed4TouchRead(TOUCH_PAD_LEFT);
    fed4TouchRead(TOUCH_PAD_CENTER);
    fed4TouchRead(TOUCH_PAD_RIGHT);
    delay(10);
  }

  fed4TouchIdleL = robustIdleAverage(TOUCH_PAD_LEFT);
  fed4TouchIdleC = robustIdleAverage(TOUCH_PAD_CENTER);
  fed4TouchIdleR = robustIdleAverage(TOUCH_PAD_RIGHT);
  if (!fed4TouchIdleL || !fed4TouchIdleC || !fed4TouchIdleR) return false;

  return fed4TouchNgApplyThresholds(fed4TouchWakeThreshold(fed4TouchIdleL),
                                    fed4TouchWakeThreshold(fed4TouchIdleC),
                                    fed4TouchWakeThreshold(fed4TouchIdleR));
}

bool fed4TouchEnableTouchpadWakeup(void) {
  return esp_sleep_enable_touchpad_wakeup() == ESP_OK;
}

const char *fed4TouchIdentifyWakePad(float triggerRise) {
  const uint32_t l = fed4TouchRead(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchRead(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchRead(TOUCH_PAD_RIGHT);

  const float fl = fed4TouchRiseFraction(l, fed4TouchIdleL);
  const float fc = fed4TouchRiseFraction(c, fed4TouchIdleC);
  const float fr = fed4TouchRiseFraction(r, fed4TouchIdleR);

  const float maxRise = max(max(fl, fc), fr);
  if (maxRise < triggerRise) return nullptr;

  if (fl >= fc && fl >= fr) return "LEFT";
  if (fc >= fr) return "CENTER";
  return "RIGHT";
}

void fed4TouchPrintDriverConfig(void) {
  Serial.println("Touch: NG direct driver + BM IIR16 + BM denoise4 + debounce1");
}

// ---------------------------------------------------------------------------
// FED4 class API
// ---------------------------------------------------------------------------

void IRAM_ATTR FED4::onTouchWakeUp() {
  // Reserved — pad ID is software rise after wake (no ISR attach).
}

bool FED4::initializeTouch() {
  if (!fed4TouchInitPads()) return false;
  touchPadLeftBaseline = fed4TouchIdleL;
  touchPadCenterBaseline = fed4TouchIdleC;
  touchPadRightBaseline = fed4TouchIdleR;
  fed4TouchEnableTouchpadWakeup();
  return true;
}

void FED4::calibrateTouchSensors(bool checkStability) {
  wakePad = 0;

  if (checkStability && fed4TouchIdleL && fed4TouchIdleC && fed4TouchIdleR) {
    const int maxReleaseAttempts = 40;
    int attempts = 0;
    while (attempts < maxReleaseAttempts && !fed4TouchPadsReleased(TOUCH_THRESHOLD)) {
      attempts++;
      delay(5);
    }
  }

  for (int i = 0; i < 8; i++) {
    fed4TouchRead(TOUCH_PAD_LEFT);
    fed4TouchRead(TOUCH_PAD_CENTER);
    fed4TouchRead(TOUCH_PAD_RIGHT);
    delay(5);
  }

  fed4TouchIdleL = robustIdleAverage(TOUCH_PAD_LEFT);
  fed4TouchIdleC = robustIdleAverage(TOUCH_PAD_CENTER);
  fed4TouchIdleR = robustIdleAverage(TOUCH_PAD_RIGHT);

  touchPadLeftBaseline = fed4TouchIdleL;
  touchPadCenterBaseline = fed4TouchIdleC;
  touchPadRightBaseline = fed4TouchIdleR;

  if (!fed4TouchIdleL || !fed4TouchIdleC || !fed4TouchIdleR) return;

  fed4TouchNgApplyThresholds(fed4TouchWakeThreshold(fed4TouchIdleL),
                             fed4TouchWakeThreshold(fed4TouchIdleC),
                             fed4TouchWakeThreshold(fed4TouchIdleR));
  fed4TouchEnableTouchpadWakeup();
  wakePad = 0;
}

/**
 * Post-wake / poke identification via max rise fraction (NG uint32_t counts).
 * Optional short hold poll for pokeDuration. Sets left/center/rightTouch flags.
 */
void FED4::interpretTouch() {
  // After light sleep, NG smooth filter needs a few samples before rise is valid
  const char *pad = nullptr;
  for (int attempt = 0; attempt < 25 && pad == nullptr; attempt++) {
    delay(2);
    pad = fed4TouchIdentifyWakePad(TOUCH_THRESHOLD);
  }
  if (pad == nullptr) {
    wakePad = 0;
    return;
  }

  const unsigned long touchStartTime = millis();
  const unsigned long maxSamplingTime_ms = 500;
  const int minReleaseReadings = 2;
  int belowThresholdCount = 0;

  while (millis() - touchStartTime < maxSamplingTime_ms) {
    if (fed4TouchPadsReleased(TOUCH_THRESHOLD)) {
      belowThresholdCount++;
      if (belowThresholdCount >= minReleaseReadings) break;
    } else {
      belowThresholdCount = 0;
    }
    delay(1);
  }

  pokeDuration = (float)(millis() - touchStartTime);

  if (strcmp(pad, "LEFT") == 0) {
    leftCount++;
    leftTouch = true;
    wakePad = 1;
  } else if (strcmp(pad, "CENTER") == 0) {
    centerCount++;
    centerTouch = true;
    wakePad = 2;
  } else if (strcmp(pad, "RIGHT") == 0) {
    rightCount++;
    rightTouch = true;
    wakePad = 3;
  }
}

void FED4::resetTouchFlags() {
  leftTouch = false;
  centerTouch = false;
  rightTouch = false;
}

void FED4::logTouchEvent() {
  if (leftTouch) {
    Serial.print("LEFT touch   ");
  } else if (centerTouch) {
    Serial.print("CENTER touch ");
  } else if (rightTouch) {
    Serial.print("RIGHT touch  ");
  }
}
