/*
 * FED4 Touch Test (ESP32-S3)
 *
 * Prints raw + smoothed touchRead() values every 100 ms. No LEDs.
 *
 * Touch pads (FED4_Pins.h):
 *   LEFT   = TOUCH_PAD_NUM1 (GPIO 1)
 *   CENTER = TOUCH_PAD_NUM3 (GPIO 3)
 *   RIGHT  = TOUCH_PAD_NUM2 (GPIO 2)
 *
 * ESP32-S3 touch notes (different from original ESP32!):
 *   - Counts INCREASE when touched (original ESP32 decreases).
 *   - touch_value_t is uint32_t. Values easily exceed 65535 — storing them
 *     in uint16_t wraps around and looks "biphasic" (rise, then apparent
 *     drop far below idle).
 *   - Sensitivity/SNR knobs: measurement cycles (touchSetCycles), charge
 *     voltage window (touch_pad_set_voltage), hardware denoise, IIR filter.
 */

#include <Arduino.h>
#include <FED4_Pins.h>
#include "driver/touch_sensor.h"

static const uint32_t PRINT_MS = 100;

// ---------------------------------------------------------------------------
// Tuning knobs
// ---------------------------------------------------------------------------

// More measure cycles = larger counts and better SNR (slightly slower reads).
// Good starting range for small/high-sensitivity targets: 1000–4000.
static const uint16_t MEASURE_CYCLES = 2000;
static const uint16_t SLEEP_CYCLES = 500;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // First reads auto-initialize the touch peripheral
  touchRead(TOUCH_PAD_LEFT);
  touchRead(TOUCH_PAD_CENTER);
  touchRead(TOUCH_PAD_RIGHT);

  // Longer measurement window boosts sensitivity for weak coupling
  touchSetCycles(MEASURE_CYCLES, SLEEP_CYCLES);

  // Wider charge/discharge voltage window = larger swing per measurement.
  // (2.7 V high, 0.5 V low, 0.5 V attenuation is a common high-SNR setting.)
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);

  // Hardware denoise: subtracts noise measured on the internal reference
  // channel (T0). Suppresses supply/temperature drift common to all pads.
  touch_pad_denoise_t denoise = {
      .grade = TOUCH_PAD_DENOISE_BIT4,
      .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
  };
  touch_pad_denoise_set_config(&denoise);
  touch_pad_denoise_enable();

  // Hardware IIR filter: smoothed values readable alongside raw
  touch_filter_config_t filter = {
      .mode = TOUCH_PAD_FILTER_IIR_16,
      .debounce_cnt = 1,
      .noise_thr = 0,
      .jitter_step = 4,
      .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
  };
  touch_pad_filter_set_config(&filter);
  touch_pad_filter_enable();

  // Warm up after reconfiguration (first reads settle)
  for (int i = 0; i < 8; i++) {
    touchRead(TOUCH_PAD_LEFT);
    touchRead(TOUCH_PAD_CENTER);
    touchRead(TOUCH_PAD_RIGHT);
    delay(10);
  }

  Serial.println("FED4 touch raw (S3: counts RISE on touch) — every 100 ms");
}

void loop() {
  uint32_t l = touchRead(TOUCH_PAD_LEFT);
  uint32_t c = touchRead(TOUCH_PAD_CENTER);
  uint32_t r = touchRead(TOUCH_PAD_RIGHT);

  uint32_t ls = 0, cs = 0, rs = 0;
  touch_pad_filter_read_smooth(TOUCH_PAD_LEFT, &ls);
  touch_pad_filter_read_smooth(TOUCH_PAD_CENTER, &cs);
  touch_pad_filter_read_smooth(TOUCH_PAD_RIGHT, &rs);

  Serial.printf("raw L:%lu C:%lu R:%lu | smooth L:%lu C:%lu R:%lu\n",
                (unsigned long)l, (unsigned long)c, (unsigned long)r,
                (unsigned long)ls, (unsigned long)cs, (unsigned long)rs);

  delay(PRINT_MS);
}
