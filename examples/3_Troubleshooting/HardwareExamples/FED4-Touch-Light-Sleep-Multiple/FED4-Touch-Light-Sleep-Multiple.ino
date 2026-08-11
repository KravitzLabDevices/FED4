/*
 * FED4 Touch Light Sleep (ESP32-S3)
 *
 * Three touch pads wake from light sleep (native touchpad wake, not EXT1 GPIO).
 * Minimal display + Serial feedback. Panel image held during light sleep via
 * VCOM keepalive between esp_light_sleep timer chunks.
 * RGB strip shows proportional touch response on wake (Serial drops in sleep).
 *
 * Touch pads (FED4_Pins.h):
 *   LEFT   = TOUCH_PAD_NUM1 (GPIO 1)
 *   CENTER = TOUCH_PAD_NUM3 (GPIO 3)
 *   RIGHT  = TOUCH_PAD_NUM2 (GPIO 2)
 *
 * ESP32-S3 touch notes (see FED4-Touch.ino / FED4-Demo-Hardware.ino):
 *   - Counts RISE when touched; values are uint32_t (not uint16_t).
 *   - Boot idle baseline + rise fraction for wake identification.
 *   - IDF 5.5+: optional direct touch_sens init (see bisection flags below).
 *
 * IDF 5.5+ bisection — verify wake + strip after each step:
 *   Step 1  FED4_TOUCH_NG_DIRECT_DRIVER=1     direct touch_sens init, NG default filter
 *   Step 2a FED4_TOUCH_NG_FLT_BM_IIR16=1      benchmark IIR-16 (default IIR-4)
 *   Step 2b FED4_TOUCH_NG_FLT_BM_DNOISE4=1   benchmark denoise_lvl 4 (default 1)
 *   Step 2c FED4_TOUCH_NG_FLT_DEBOUNCE1=1     data debounce 1 (default 2)
 *   Step 3  FED4_TOUCH_NG_HW_DENOISE=1        T0 subtractive denoise (breaks on FED4 — leave 0)
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display pins).
 * Serial optional: 500 ms grace delay, then runs without a monitor attached.
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>
#include <FED4_Pins.h>
#include <FED4_DisplayOrient.h>
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
// Step 0: all 0 — Arduino touchSetTiming / touchRead / touchAttachInterrupt.
#define FED4_TOUCH_NG_DIRECT_DRIVER 1   // Step 1
#define FED4_TOUCH_NG_FLT_BM_IIR16 1      // Step 2a — ON
#define FED4_TOUCH_NG_FLT_BM_DNOISE4 1     // Step 2b — ON
#define FED4_TOUCH_NG_FLT_DEBOUNCE1 1       // Step 2c — ON
#define FED4_TOUCH_NG_HW_DENOISE 0          // Step 3 — OFF (T0 denoise breaks wake/sensitivity here)
#if FED4_TOUCH_NG_DIRECT_DRIVER
#include "driver/touch_sens.h"
#endif
#else
#include "driver/touch_sensor.h"
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
  {                         \
    int16_t t = a;          \
    a = b;                  \
    b = t;                  \
  }
#endif

static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;
static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;
static const uint32_t SPI_HZ = 1000000;

static const uint32_t VCOM_MS = 500;
static const uint32_t SERIAL_BOOT_DELAY_MS = 1000;
static const uint32_t STRIP_MS = 8;
static const uint32_t TOUCH_LED_MIN_MS = 400;
static const uint32_t TOUCH_LED_MAX_MS = 3000;

// S3 touch tuning (FED4-Touch.ino)
static const uint16_t TOUCH_MEASURE_CYCLES = 2000;
static const uint16_t TOUCH_SLEEP_CYCLES = 500;
static const float TOUCH_TRIGGER_RISE = 0.03f;  // wake threshold — 3% rise above idle
// Strip brightness mapping (FED4-Demo-Hardware.ino)
static const float TOUCH_LED_FULL_RISE = 0.05f;
static const float TOUCH_DEADBAND = 0.015f;
static const uint8_t TOUCH_LED_MIN_BRIGHT = 28;
static const uint8_t STRIP_BRIGHTNESS = 120;

#define NUM_STRIP_LEDS 8

static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel strip(NUM_STRIP_LEDS, RGB_STRIP, NEO_GRB + NEO_KHZ800);

uint32_t touchIdleL = 0, touchIdleC = 0, touchIdleR = 0;
uint32_t wakeCount = 0;

// ---------------------------------------------------------------------------
// MIP display (minimal, from FED4-SleepModes / FED4-Display-Standalone)
// ---------------------------------------------------------------------------

class MIPDisplay : public Adafruit_GFX {
public:
  MIPDisplay() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

  bool begin() {
    const uint32_t bufferSize = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8;
    if (frameBuffer) {
      free(frameBuffer);
      frameBuffer = nullptr;
    }
    frameBuffer = (uint8_t *)malloc(bufferSize);
    if (!frameBuffer) return false;
    memset(frameBuffer, 0x00, bufferSize);
    setRotation(FED4_DISPLAY_ROTATION_NATIVE);
    return true;
  }

  void clearBlack() {
    memset(frameBuffer, 0x00, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void refresh() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
    delay(4);
    delayMicroseconds(30);
    digitalWrite(DISPLAY_CS, HIGH);
    delay(5);

    SPI.beginTransaction(SPISettings(SPI_HZ, LSBFIRST, SPI_MODE0));
    const uint8_t bytesPerLine = PANEL_WIDTH / 8;
    for (uint8_t line = 0; line < PANEL_HEIGHT; line++) {
      SPI.transfer(line);
      const uint8_t *row = frameBuffer + (uint32_t)line * bytesPerLine;
      for (uint8_t b = 0; b < bytesPerLine; b++) SPI.transfer(row[b]);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
    }
    SPI.endTransaction();
    delay(2);
    digitalWrite(DISPLAY_CS, LOW);
  }

  void toggleVcomKeepAlive() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
  }

  void setVcomLow() {
    vcomState = false;
    digitalWrite(DISPLAY_VCOM, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

    int16_t px = x, py = y;
    switch (rotation) {
      case 1:
        _swap_int16_t(px, py);
        px = PANEL_WIDTH - 1 - px;
        break;
      case 2:
        px = PANEL_WIDTH - 1 - px;
        py = PANEL_HEIGHT - 1 - py;
        break;
      case 3:
        _swap_int16_t(px, py);
        py = PANEL_HEIGHT - 1 - py;
        break;
      default:
        break;
    }

    if (color)
      frameBuffer[(py * PANEL_WIDTH + px) / 8] |= pgm_read_byte(&set[px & 7]);
    else
      frameBuffer[(py * PANEL_WIDTH + px) / 8] &= pgm_read_byte(&clr[px & 7]);
  }

private:
  uint8_t *frameBuffer = nullptr;
  bool vcomState = false;
};

MIPDisplay display;

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void displayReset() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  pinMode(DISPLAY_VCOM, OUTPUT);
  display.setVcomLow();
  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

void drawScreen(const char *status, const char *detail) {
  display.clearBlack();
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(8, 20);
  display.print("Touch Light Sleep");
  display.setCursor(8, 50);
  display.print(status);
  if (detail && detail[0]) {
    display.setCursor(8, 70);
    display.print(detail);
  }
  display.setCursor(8, 100);
  display.printf("Wakes: %lu", (unsigned long)wakeCount);
  display.refresh();
}

// ---------------------------------------------------------------------------
// Touch — S3 tuning + boot idle baseline (FED4-Touch / FED4-Demo-Hardware)
// ---------------------------------------------------------------------------

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER

static touch_sensor_handle_t sTouchSens = NULL;
static touch_channel_handle_t sTouchChanLeft = NULL;
static touch_channel_handle_t sTouchChanCenter = NULL;
static touch_channel_handle_t sTouchChanRight = NULL;
static touch_channel_handle_t sTouchChanByPad[TOUCH_PAD_MAX] = {};

static touch_sensor_filter_config_t fed4TouchFilterConfig() {
  // Start from NG defaults, then apply enabled bisection overrides one at a time.
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

static touch_denoise_chan_config_t fed4TouchDenoiseConfig() {
  // Maps old touch_pad_denoise: grade BIT4 + cap L4 (~10.6 pF on T0).
  return (touch_denoise_chan_config_t){
      .charge_speed = TOUCH_CHARGE_SPEED_7,
      .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
      .ref_cap = TOUCH_DENOISE_CHAN_CAP_10PF,
      .resolution = TOUCH_DENOISE_CHAN_RESOLUTION_BIT4,
  };
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

#if FED4_TOUCH_NG_HW_DENOISE && SOC_TOUCH_SUPPORT_DENOISE_CHAN
  // Bisection: T0 subtractive denoise — disable first if sensitivity drops.
  touch_denoise_chan_config_t denoise_cfg = fed4TouchDenoiseConfig();
  if (touch_sensor_config_denoise_channel(sTouchSens, &denoise_cfg) != ESP_OK)
    return false;
#endif

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

static uint32_t fed4TouchRead(uint8_t pin) {
  const int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0 || !sTouchChanByPad[pad]) return 0;

  uint32_t value[TOUCH_SAMPLE_CFG_NUM] = {};
  if (touch_channel_read_data(sTouchChanByPad[pad], TOUCH_CHAN_DATA_TYPE_SMOOTH, value) != ESP_OK)
    return 0;
  return value[0];
}

#define FED4_TOUCH_READ(pin) fed4TouchRead(pin)

#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)

#define FED4_TOUCH_READ(pin) touchRead(pin)

#else  // IDF 5.5+, Arduino NG touch HAL

#define FED4_TOUCH_READ(pin) touchRead(pin)

#endif

static float touchRiseFraction(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0.0f;
  return (float)(raw - idle) / (float)idle;
}

uint32_t robustIdleAverage(uint8_t pad) {
  const int samples = 16;
  uint64_t sum = 0;
  uint32_t lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < samples; i++) {
    uint32_t v = FED4_TOUCH_READ(pad);
    sum += v;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
    delay(5);
  }
  return (uint32_t)((sum - lo - hi) / (samples - 2));
}

// Hardware wake threshold from boot idle. IDF 5.5+ expects a delta above benchmark;
// legacy driver expects an absolute count (idle + rise).
static uint32_t touchWakeThreshold(uint32_t idle) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  return (uint32_t)(idle * TOUCH_TRIGGER_RISE);
#else
  return idle + (uint32_t)(idle * TOUCH_TRIGGER_RISE);
#endif
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0) || !FED4_TOUCH_NG_DIRECT_DRIVER
void IRAM_ATTR onTouchLeft() {}
void IRAM_ATTR onTouchCenter() {}
void IRAM_ATTR onTouchRight() {}
#endif

static void configureTouchHardware() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#if !FED4_TOUCH_NG_DIRECT_DRIVER
  // Must be before the first touchRead().
  touchSetTiming(32.0f, TOUCH_SLEEP_CYCLES);
  touchSetConfig(TOUCH_MEASURE_CYCLES, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V7);
#endif
#else
  touchSetCycles(TOUCH_MEASURE_CYCLES, TOUCH_SLEEP_CYCLES);
#endif
}

bool initTouchPads() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  if (!fed4TouchNgCreateController()) return false;
#else
  configureTouchHardware();

  // First reads auto-initialize the touch peripheral
  FED4_TOUCH_READ(TOUCH_PAD_LEFT);
  FED4_TOUCH_READ(TOUCH_PAD_CENTER);
  FED4_TOUCH_READ(TOUCH_PAD_RIGHT);

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
    FED4_TOUCH_READ(TOUCH_PAD_LEFT);
    FED4_TOUCH_READ(TOUCH_PAD_CENTER);
    FED4_TOUCH_READ(TOUCH_PAD_RIGHT);
    delay(10);
  }

  touchIdleL = robustIdleAverage(TOUCH_PAD_LEFT);
  touchIdleC = robustIdleAverage(TOUCH_PAD_CENTER);
  touchIdleR = robustIdleAverage(TOUCH_PAD_RIGHT);
  if (!touchIdleL || !touchIdleC || !touchIdleR) return false;

  const uint32_t threshL = touchWakeThreshold(touchIdleL);
  const uint32_t threshC = touchWakeThreshold(touchIdleC);
  const uint32_t threshR = touchWakeThreshold(touchIdleR);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) && FED4_TOUCH_NG_DIRECT_DRIVER
  if (!fed4TouchNgApplyThresholds(threshL, threshC, threshR)) return false;
#else
  touchAttachInterrupt(TOUCH_PAD_LEFT, onTouchLeft, threshL);
  touchAttachInterrupt(TOUCH_PAD_CENTER, onTouchCenter, threshC);
  touchAttachInterrupt(TOUCH_PAD_RIGHT, onTouchRight, threshR);
#endif

  esp_sleep_enable_touchpad_wakeup();
  return true;
}

const char *identifyWakePad() {
  const uint32_t l = FED4_TOUCH_READ(TOUCH_PAD_LEFT);
  const uint32_t c = FED4_TOUCH_READ(TOUCH_PAD_CENTER);
  const uint32_t r = FED4_TOUCH_READ(TOUCH_PAD_RIGHT);

  const float fl = touchRiseFraction(l, touchIdleL);
  const float fc = touchRiseFraction(c, touchIdleC);
  const float fr = touchRiseFraction(r, touchIdleR);

  const float maxRise = max(max(fl, fc), fr);
  if (maxRise < TOUCH_TRIGGER_RISE) return nullptr;

  if (fl >= fc && fl >= fr) return "LEFT";
  if (fc >= fr) return "CENTER";
  return "RIGHT";
}

// Light sleep until touch; timer chunks keep MIP VCOM alive (FED4-SleepModes).
void lightSleepUntilTouch() {
  while (true) {
    display.toggleVcomKeepAlive();
    esp_sleep_enable_timer_wakeup((uint64_t)VCOM_MS * 1000ULL);
    Serial.flush();
    esp_light_sleep_start();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TOUCHPAD) break;
  }
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

// ---------------------------------------------------------------------------
// RGB strip — proportional touch glow (FED4-Demo-Hardware.ino)
// 0-2 right (blue), 3-4 center (green), 5-7 left (red)
// ---------------------------------------------------------------------------

uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  return strip.Color(r * bright / 255, g * bright / 255, b * bright / 255);
}

uint8_t touchRiseToBrightness(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0;

  const float rise = (float)(raw - idle) / (float)idle;
  if (rise < TOUCH_DEADBAND) return 0;

  const float span = TOUCH_LED_FULL_RISE - TOUCH_DEADBAND;
  float t = (rise - TOUCH_DEADBAND) / span;
  if (t > 1.0f) t = 1.0f;
  t = powf(t, 0.75f);

  uint8_t bright = (uint8_t)(t * 255.0f);
  if (bright > 0 && bright < TOUCH_LED_MIN_BRIGHT)
    bright = TOUCH_LED_MIN_BRIGHT;
  return bright;
}

void updateTouchStrip() {
  const uint32_t l = FED4_TOUCH_READ(TOUCH_PAD_LEFT);
  const uint32_t c = FED4_TOUCH_READ(TOUCH_PAD_CENTER);
  const uint32_t r = FED4_TOUCH_READ(TOUCH_PAD_RIGHT);

  const uint8_t brightL = touchRiseToBrightness(l, touchIdleL);
  const uint8_t brightC = touchRiseToBrightness(c, touchIdleC);
  const uint8_t brightR = touchRiseToBrightness(r, touchIdleR);

  for (int i = 0; i < 3; i++)
    strip.setPixelColor(i, scaleColor(0, 0, 255, brightR));
  for (int i = 3; i < 5; i++)
    strip.setPixelColor(i, scaleColor(0, 255, 0, brightC));
  for (int i = 5; i < NUM_STRIP_LEDS; i++)
    strip.setPixelColor(i, scaleColor(255, 0, 0, brightL));
  strip.show();
}

bool anyStripTouchActive() {
  const uint32_t l = FED4_TOUCH_READ(TOUCH_PAD_LEFT);
  const uint32_t c = FED4_TOUCH_READ(TOUCH_PAD_CENTER);
  const uint32_t r = FED4_TOUCH_READ(TOUCH_PAD_RIGHT);
  return touchRiseToBrightness(l, touchIdleL) ||
         touchRiseToBrightness(c, touchIdleC) ||
         touchRiseToBrightness(r, touchIdleR);
}

// Poll strip while pads are held; min/max window for sensitivity checks.
void runTouchLedFeedback() {
  const unsigned long startMs = millis();
  unsigned long lastTouchMs = startMs;

  while (true) {
    updateTouchStrip();

    if (anyStripTouchActive())
      lastTouchMs = millis();

    const unsigned long now = millis();
    if (now - startMs >= TOUCH_LED_MAX_MS) break;
    if (now - lastTouchMs >= TOUCH_LED_MIN_MS && now - startMs >= TOUCH_LED_MIN_MS) break;

    delay(STRIP_MS);
  }

  strip.clear();
  strip.show();
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void printTouchBisectionStep() {
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
  Serial.println("Touch: Arduino NG HAL (step 0)");
#endif
}
#endif

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 fail");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW);
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delay(5);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  displayReset();
  displayLight(false);

  if (!display.begin()) {
    Serial.println("Display fail");
    while (1) delay(10);
  }

  if (!initTouchPads()) {
    Serial.println("Touch fail — keep pads clear at boot");
    while (1) delay(10);
  }

  strip.begin();
  strip.setBrightness(STRIP_BRIGHTNESS);
  strip.clear();
  strip.show();

  Serial.printf("Ready idle L:%lu C:%lu R:%lu\n",
                (unsigned long)touchIdleL, (unsigned long)touchIdleC,
                (unsigned long)touchIdleR);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  printTouchBisectionStep();
#endif
}

void loop() {
  strip.clear();
  strip.show();
  drawScreen("Sleeping", "Touch any pad");
  lightSleepUntilTouch();

  wakeCount++;
  const char *pad = identifyWakePad();
  if (pad) {
    Serial.printf("Wake: %s\n", pad);
    drawScreen("Wake", pad);
  } else {
    Serial.println("Wake: touch");
    drawScreen("Wake", "?");
  }

  runTouchLedFeedback();
}
