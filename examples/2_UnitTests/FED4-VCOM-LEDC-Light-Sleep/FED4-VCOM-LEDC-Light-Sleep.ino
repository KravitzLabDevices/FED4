/*
 * FED4 VCOM LEDC Light-Sleep Bench Test
 *
 * Isolates Kyocera TN0216 VCOM keepalive via LEDC PWM that should keep
 * running through esp_light_sleep (no 500 ms CPU toggle chunks).
 *
 * Standalone — no FED4.h. Does not refresh the panel; leftover MIP pixels
 * stay as last written. MCP is used only to hold RST LOW so VCOM may pulse
 * (datasheet: VCOM must be LOW while RST is HIGH).
 *
 * Scope: GPIO 43 (DISPLAY_VCOM) vs GND. Expect ~2 Hz, 50% square, awake
 * and during the 10 s light-sleep window.
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display).
 * Do not use analogWrite() in this sketch — S3 LEDC timers share one clock.
 *
 * Report back:
 *   1. Compile: KEEP_ALIVE available? ledc_timer / channel ESP_OK?
 *   2. Serial: IDF version, measured ledc_get_freq(), sleep return + cause
 *   3. Scope awake: clean ~2 Hz 50% on GPIO 43?
 *   4. Scope in light sleep: PWM continues, or freezes / goes Hi-Z?
 *   5. Wake: cause=TIMER and elapsed ~10 s (not an instant return)?
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_idf_version.h"
#include "esp_err.h"

// Use LEDC_USE_RC_FAST_CLK (ledc_clk_cfg_t / soc_periph_ledc_clk_src_legacy_t).
// Do NOT alias LEDC_SLOW_CLK_RC_FAST — that is ledc_slow_clk_sel_t.
// Do NOT #ifdef LEDC_SLEEP_MODE_KEEP_ALIVE / LEDC_USE_RC_FAST_CLK — those are
// enum enumerators, not macros; #ifdef is always false and silently drops
// KEEP_ALIVE (default sleep_mode = no PWM in light sleep).

static const uint32_t SERIAL_BOOT_DELAY_MS = 1000;
static const uint32_t AWAKE_MS = 2000;
static const uint32_t SLEEP_MS = 10000;
static const uint32_t VCOM_HZ = 2;
static const ledc_timer_t VCOM_TIMER = LEDC_TIMER_1;
static const ledc_channel_t VCOM_CHANNEL = LEDC_CHANNEL_1;
static const ledc_mode_t VCOM_SPEED = LEDC_LOW_SPEED_MODE;
static const ledc_timer_bit_t VCOM_RES = LEDC_TIMER_14_BIT;
static const uint32_t VCOM_DUTY_50 = 8192; // 50% of 2^14

Adafruit_MCP23X17 mcp;
uint32_t cycle = 0;

static const char *wakeCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "UNDEFINED";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "TIMER";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "TOUCHPAD";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "GPIO";
    case ESP_SLEEP_WAKEUP_UART:
      return "UART";
    default:
      return "OTHER";
  }
}

static bool holdDisplayResetLow() {
  Wire.begin(SDA, SCL, 100000);
  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed — abort (will not pulse VCOM with RST unknown)");
    return false;
  }

  // VCOM LOW before RST HIGH (TN0216 section 6-2 shoot-through)
  pinMode(DISPLAY_VCOM, OUTPUT);
  digitalWrite(DISPLAY_VCOM, LOW);

  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);

  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, LOW);

  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);

  Serial.println("Display RST held LOW (panel ON). Frontlight off. SCS LOW.");
  return true;
}

static bool setupVcomLedc() {
  // Keep RC_FAST powered in light sleep (LEDC clock for KEEP_ALIVE)
  esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);
  Serial.printf("esp_sleep_pd_config(RC_FAST, ON): %s\n", esp_err_to_name(err));

  ledc_timer_config_t timer = {};
  timer.speed_mode = VCOM_SPEED;
  timer.timer_num = VCOM_TIMER;
  timer.duty_resolution = VCOM_RES;
  timer.freq_hz = VCOM_HZ;
  timer.clk_cfg = (ledc_clk_cfg_t)LEDC_USE_RC_FAST_CLK;

  err = ledc_timer_config(&timer);
  Serial.printf("ledc_timer_config: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) {
    return false;
  }

  ledc_channel_config_t channel = {};
  channel.gpio_num = DISPLAY_VCOM;
  channel.speed_mode = VCOM_SPEED;
  channel.channel = VCOM_CHANNEL;
  channel.timer_sel = VCOM_TIMER;
  channel.duty = VCOM_DUTY_50;
  channel.hpoint = 0;
  channel.sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE;

  err = ledc_channel_config(&channel);
  Serial.printf("ledc_channel_config: %s (sleep_mode=KEEP_ALIVE=%d)\n",
                esp_err_to_name(err), (int)LEDC_SLEEP_MODE_KEEP_ALIVE);
  if (err != ESP_OK) {
    return false;
  }

  // Prevent GPIO sleep mux from freezing/holding the pin (required even with KEEP_ALIVE)
  err = gpio_sleep_sel_dis((gpio_num_t)DISPLAY_VCOM);
  Serial.printf("gpio_sleep_sel_dis(%d): %s\n", DISPLAY_VCOM, esp_err_to_name(err));

  const uint32_t measured = ledc_get_freq(VCOM_SPEED, VCOM_TIMER);
  Serial.printf("ledc_get_freq: %lu Hz (requested %lu)\n",
                (unsigned long)measured, (unsigned long)VCOM_HZ);
  return measured > 0;
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);

  Serial.println("=== FED4 VCOM LEDC Light-Sleep ===");
  Serial.printf("IDF %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR,
                ESP_IDF_VERSION_PATCH);
#ifdef ESP_ARDUINO_VERSION_MAJOR
  Serial.printf("Arduino-ESP32 %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
#endif
  Serial.printf("VCOM GPIO %d  %lu Hz  50%%  timer %d  channel %d  RC_FAST + KEEP_ALIVE\n",
                DISPLAY_VCOM, (unsigned long)VCOM_HZ, (int)VCOM_TIMER,
                (int)VCOM_CHANNEL);
  Serial.printf("Cycle: awake %lu ms, light sleep %lu ms, timer wake only\n",
                (unsigned long)AWAKE_MS, (unsigned long)SLEEP_MS);

  if (!holdDisplayResetLow()) {
    while (true) {
      delay(1000);
    }
  }
  if (!setupVcomLedc()) {
    Serial.println("LEDC setup failed — abort");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Scope GPIO 43 now (awake PWM). Then watch through sleep.");
}

void loop() {
  cycle++;
  digitalWrite(STATUS_LED, HIGH);
  Serial.printf("\n--- cycle %lu awake %lu ms ---\n", (unsigned long)cycle,
                (unsigned long)AWAKE_MS);
  Serial.flush();
  delay(AWAKE_MS);

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_MS * 1000ULL);

  digitalWrite(STATUS_LED, LOW);
  Serial.printf("light sleep %lu ms...\n", (unsigned long)SLEEP_MS);
  Serial.flush();

  const uint32_t t0 = millis();
  const esp_err_t sleepErr = esp_light_sleep_start();
  const uint32_t elapsed = millis() - t0;
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  digitalWrite(STATUS_LED, HIGH);
  Serial.printf("esp_light_sleep_start: %s\n", esp_err_to_name(sleepErr));
  Serial.printf("wake cause: %s (%d)\n", wakeCauseName(cause), (int)cause);
  Serial.printf("elapsed: %lu ms (expect ~%lu)\n", (unsigned long)elapsed,
                (unsigned long)SLEEP_MS);
  Serial.printf("ledc_get_freq after wake: %lu Hz\n",
                (unsigned long)ledc_get_freq(VCOM_SPEED, VCOM_TIMER));
  Serial.flush();
}
