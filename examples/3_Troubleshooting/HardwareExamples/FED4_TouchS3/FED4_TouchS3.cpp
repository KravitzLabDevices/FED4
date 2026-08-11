#include "FED4_TouchS3.h"

#include <Arduino.h>
#include <FED4_Pins.h>
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#define FED4_TOUCH_NG_DIRECT_DRIVER 1
#define FED4_TOUCH_NG_FLT_BM_IIR16 1
#define FED4_TOUCH_NG_FLT_BM_DNOISE4 1
#define FED4_TOUCH_NG_FLT_DEBOUNCE1 1
#define FED4_TOUCH_NG_HW_DENOISE 0
#if FED4_TOUCH_NG_DIRECT_DRIVER
#include "driver/touch_sens.h"
#endif
#else
#include "driver/touch_sensor.h"
#endif

static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
static const uint16_t TOUCH_SLEEP_CYCLES = 500;

uint32_t fed4TouchIdleL = 0;
uint32_t fed4TouchIdleC = 0;
uint32_t fed4TouchIdleR = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER

static touch_sensor_handle_t sTouchSens = NULL;
static touch_channel_handle_t sTouchChanLeft = NULL;
static touch_channel_handle_t sTouchChanCenter = NULL;
static touch_channel_handle_t sTouchChanRight = NULL;
static touch_channel_handle_t sTouchChanByPad[TOUCH_PAD_MAX] = {};

static touch_sensor_filter_config_t fed4TouchFilterConfig() {
  touch_sensor_filter_config_t cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
#if FED4_TOUCH_NG_FLT_BM_IIR16
  cfg.benchmark.filter_mode = TOUCH_BM_IIR_FILTER_16;
#endif
#if FED4_TOUCH_NG_FLT_BM_DNOISE4
  cfg.benchmark.denoise_lvl = 4;
#endif
#if FED4_TOUCH_NG_FLT_DEBOUNCE1
  cfg.data.debounce_cnt = 1;
#endif
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

  if (!fed4TouchNgAddChannel(TOUCH_PAD_NUM1, &sTouchChanLeft)) return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_NUM3, &sTouchChanCenter)) return false;
  if (!fed4TouchNgAddChannel(TOUCH_PAD_NUM2, &sTouchChanRight)) return false;

#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_LSLP_CONFIG();
  if (touch_sensor_config_sleep_wakeup(sTouchSens, &sleep_cfg) != ESP_OK) return false;
#endif

  if (!fed4TouchNgEnableStart()) return false;

  touch_sensor_filter_config_t filter_cfg = fed4TouchFilterConfig();
  return touch_sensor_config_filter(sTouchSens, &filter_cfg) == ESP_OK;
}

static bool fed4TouchNgApplyThresholds(uint32_t threshL, uint32_t threshC, uint32_t threshR) {
  const int pads[] = {TOUCH_PAD_NUM1, TOUCH_PAD_NUM3, TOUCH_PAD_NUM2};
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
  if (pad < 0 || !sTouchChanByPad[pad]) return 0;

  uint32_t value[TOUCH_SAMPLE_CFG_NUM] = {};
  if (touch_channel_read_data(sTouchChanByPad[pad], type, value) != ESP_OK)
    return 0;
  return value[0];
}

#endif  // IDF 5.5+ direct driver

static uint32_t fed4TouchReadRaw(uint8_t pin) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  return fed4TouchNgReadChannel(pin, TOUCH_CHAN_DATA_TYPE_SMOOTH);
#else
  return touchRead(pin);
#endif
}

static void configureTouchHardware() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#if !FED4_TOUCH_NG_DIRECT_DRIVER
  touchSetTiming(32.0f, TOUCH_SLEEP_CYCLES);
  touchSetConfig(TOUCH_MEASURE_CYCLES, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V7);
#endif
#else
  touchSetCycles(TOUCH_MEASURE_CYCLES, TOUCH_SLEEP_CYCLES);
#endif
}

static uint32_t robustIdleAverage(uint8_t pad) {
  const int samples = 16;
  uint64_t sum = 0;
  uint32_t lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < samples; i++) {
    uint32_t v = fed4TouchReadRaw(pad);
    sum += v;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
    delay(5);
  }
  return (uint32_t)((sum - lo - hi) / (samples - 2));
}

float fed4TouchS3RiseFraction(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

uint32_t fed4TouchS3WakeThreshold(uint32_t idle) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  return (uint32_t)(idle * 0.03f);
#else
  return idle + (uint32_t)(idle * 0.03f);
#endif
}

uint32_t fed4TouchS3Read(uint8_t pin) {
  return fed4TouchReadRaw(pin);
}

uint32_t fed4TouchS3ReadSmooth(uint8_t pin) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  return fed4TouchNgReadChannel(pin, TOUCH_CHAN_DATA_TYPE_SMOOTH);
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
  uint32_t smooth = 0;
  touch_pad_filter_read_smooth((touch_pad_t)digitalPinToTouchChannel(pin), &smooth);
  return smooth;
#else
  return touchRead(pin);
#endif
}

bool fed4TouchS3InitPads() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  if (!fed4TouchNgCreateController()) return false;
#else
  configureTouchHardware();

  fed4TouchReadRaw(TOUCH_PAD_LEFT);
  fed4TouchReadRaw(TOUCH_PAD_CENTER);
  fed4TouchReadRaw(TOUCH_PAD_RIGHT);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);

  touch_pad_denoise_t denoise = {
      .grade = TOUCH_PAD_DENOISE_BIT4,
      .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
  };
  touch_pad_denoise_set_config(&denoise);
  touch_pad_denoise_enable();

  touch_filter_config_t filter = {
      .mode = TOUCH_PAD_FILTER_IIR_16,
      .debounce_cnt = 1,
      .noise_thr = 0,
      .jitter_step = 4,
      .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
  };
  touch_pad_filter_set_config(&filter);
  touch_pad_filter_enable();
#endif
#endif

  for (int i = 0; i < 8; i++) {
    fed4TouchReadRaw(TOUCH_PAD_LEFT);
    fed4TouchReadRaw(TOUCH_PAD_CENTER);
    fed4TouchReadRaw(TOUCH_PAD_RIGHT);
    delay(10);
  }

  fed4TouchIdleL = robustIdleAverage(TOUCH_PAD_LEFT);
  fed4TouchIdleC = robustIdleAverage(TOUCH_PAD_CENTER);
  fed4TouchIdleR = robustIdleAverage(TOUCH_PAD_RIGHT);
  if (!fed4TouchIdleL || !fed4TouchIdleC || !fed4TouchIdleR) return false;

  const uint32_t threshL = fed4TouchS3WakeThreshold(fed4TouchIdleL);
  const uint32_t threshC = fed4TouchS3WakeThreshold(fed4TouchIdleC);
  const uint32_t threshR = fed4TouchS3WakeThreshold(fed4TouchIdleR);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  return fed4TouchNgApplyThresholds(threshL, threshC, threshR);
#else
  return true;
#endif
}

bool fed4TouchS3EnableTouchpadWakeup() {
  return esp_sleep_enable_touchpad_wakeup() == ESP_OK;
}

const char *fed4TouchS3IdentifyWakePad(float triggerRise) {
  const uint32_t l = fed4TouchS3Read(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchS3Read(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchS3Read(TOUCH_PAD_RIGHT);

  const float fl = fed4TouchS3RiseFraction(l, fed4TouchIdleL);
  const float fc = fed4TouchS3RiseFraction(c, fed4TouchIdleC);
  const float fr = fed4TouchS3RiseFraction(r, fed4TouchIdleR);

  const float maxRise = max(max(fl, fc), fr);
  if (maxRise < triggerRise) return nullptr;

  if (fl >= fc && fl >= fr) return "LEFT";
  if (fc >= fr) return "CENTER";
  return "RIGHT";
}

#if defined(ARDUINO) && defined(ESP_PLATFORM)
void fed4TouchS3PrintDriverConfig() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#if FED4_TOUCH_NG_DIRECT_DRIVER
  Serial.print("Touch: NG direct driver, default filter");
#if FED4_TOUCH_NG_FLT_BM_IIR16
  Serial.print(" + BM IIR16");
#endif
#if FED4_TOUCH_NG_FLT_BM_DNOISE4
  Serial.print(" + BM denoise4");
#endif
#if FED4_TOUCH_NG_FLT_DEBOUNCE1
  Serial.print(" + debounce1");
#endif
#if FED4_TOUCH_NG_HW_DENOISE
  Serial.print(" + T0 denoise");
#endif
  Serial.println();
#else
  Serial.println("Touch: Arduino NG HAL");
#endif
#else
  Serial.println("Touch: legacy touch_pad driver");
#endif
}
#endif
