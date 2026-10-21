/*
 * FED4 PIR Counter Test (S-35770E01)
 *
 * Standalone bring-up for the counter addon. Does not call FED4::sleep()
 * or startSleep(), and does not modify the library. Light sleep here is
 * esp_light_sleep_start(), the same ESP-IDF call the idle path uses.
 *
 * J5 (SCL, SDA, +3.0 V, GND) plugs into a main I2C port. Bus is SDA GPIO 8,
 * SCL GPIO 9, always on, same bus as the RTC. Address 0x32. This sketch does
 * not switch PSV2 or PSV3. If 0x32 NACKs after deep sleep, the socket lost
 * power.
 *
 * COUNT_CLK (TP1) is CLKIN. Each rising edge counts +1, so 10 Hz is 10
 * counts per second, not 20. R23 (100k) holds CLKIN low when TP1 is open.
 * C17 (10 nF) slows edges; keep the test wave slow (about 1-100 Hz).
 *
 * Bench: function generator, or any push-pull source, on TP1 to GND.
 * 10.0 Hz, 50% duty, 0-3.0 V (EXPECTED_HZ below). VIH minimum is about
 * 2.1 V. Do not exceed 5.5 V.
 * Generator OFF (TP1 open) for the quiet check. Leave it running through
 * the awake, light-sleep, and deep-sleep windows.
 *
 * RST is pulled up on the board (R13). The sketch resets the count over I2C.
 *
 * A PASS does not mean the PIR output is wired. The board note "wire tap to
 * PIR output" is a later step, not this test.
 *
 * Serial 115200. Open the monitor before reset so you see the quiet check
 * and the countdown. Deep sleep reboots the ESP32; the result prints when
 * the monitor reconnects. The S-35770 does not reboot.
 *
 * Deep-sleep PASS uses the measured time between the two count samples.
 * The counter keeps running while the ESP32 wakes and boots, so that
 * interval is a little longer than the 10 s timer. Tolerance stays +/-3.
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <FED4_Pins.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static const uint8_t COUNTER_ADDR = 0x32;
static const float EXPECTED_HZ = 10.0f;
static const uint32_t WINDOW_MS = 10000;
static const uint32_t QUIET_MS = 5000;
static const uint32_t COUNT_TOLERANCE = 3;
static const uint32_t COUNTER_MOD = 0x01000000u;
static const uint32_t DEEP_MAGIC = 0x53353770u; // retained only across deep sleep

static const uint8_t RESET_POINTER = 0x81; // B7 address pointer, B0 test bit
static const uint8_t RESET_RST = 0x02;     // RST2:0 = 010

RTC_DATA_ATTR uint32_t deepMagic = 0;
RTC_DATA_ATTR uint32_t countBeforeDeep = 0;
RTC_DATA_ATTR uint32_t expectedDeepDelta = 0;
RTC_DATA_ATTR uint64_t rtcUsBeforeDeep = 0;

static char holdLine[240];
static bool holdResult = false;

// RTC-backed time. Keeps running through deep sleep, unlike millis().
uint64_t rtcNowUs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

uint32_t expectedInWindow() {
  return (uint32_t)(EXPECTED_HZ * (float)(WINDOW_MS / 1000) + 0.5f);
}

uint32_t deltaCount(uint32_t before, uint32_t after) {
  if (after >= before) return after - before;
  return (COUNTER_MOD - before) + after;
}

const char *wakeCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER: return "timer";
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "undefined";
    default: return "other";
  }
}

void setHold(const char *line) {
  strncpy(holdLine, line, sizeof(holdLine) - 1);
  holdLine[sizeof(holdLine) - 1] = '\0';
  holdResult = true;
}

bool counterPresent() {
  Wire.beginTransmission(COUNTER_ADDR);
  return Wire.endTransmission() == 0;
}

bool readCount(uint32_t &count) {
  uint8_t got = Wire.requestFrom(COUNTER_ADDR, (uint8_t)3);
  if (got != 3) return false;
  uint32_t msb = Wire.read();
  uint32_t mid = Wire.read();
  uint32_t lsb = Wire.read();
  count = (msb << 16) | (mid << 8) | lsb;
  return true;
}

bool resetCounter() {
  Wire.beginTransmission(COUNTER_ADDR);
  Wire.write(RESET_POINTER);
  Wire.write((uint8_t)0x00);
  Wire.write((uint8_t)0x00);
  Wire.write(RESET_RST);
  return Wire.endTransmission() == 0;
}

void reinitBus() {
  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);
}

// Fills buf with the one-line result. Returns true when delta is in tolerance.
bool formatDelta(char *buf, size_t buflen, const char *name, uint32_t seconds,
                 uint32_t before, uint32_t after, uint32_t expect, uint32_t tol) {
  uint32_t delta = deltaCount(before, after);
  uint32_t err = (delta > expect) ? (delta - expect) : (expect - delta);
  bool pass = err <= tol;
  float hz = (seconds > 0) ? ((float)delta / (float)seconds) : 0.0f;
  snprintf(buf, buflen,
           "%s %lus: before=%lu after=%lu delta=%lu expect=%lu tol=+/-%lu implied=%.2f Hz %s",
           name, (unsigned long)seconds, (unsigned long)before, (unsigned long)after,
           (unsigned long)delta, (unsigned long)expect, (unsigned long)tol, hz,
           pass ? "PASS" : "FAIL");
  return pass;
}

bool logDelta(const char *name, uint32_t seconds, uint32_t before, uint32_t after,
              uint32_t expect, uint32_t tol) {
  char line[200];
  bool pass = formatDelta(line, sizeof(line), name, seconds, before, after, expect, tol);
  Serial.println(line);
  return pass;
}

bool sample(const char *what, uint32_t &count) {
  if (readCount(count)) return true;
  char line[120];
  snprintf(line, sizeof(line), "%s: I2C read failed. FAIL", what);
  Serial.println(line);
  setHold(line);
  return false;
}

void armTimerWake(uint32_t ms) {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
}

void reportDeepWake() {
  // Sample before Serial.begin. USB reconnect can take longer than the
  // +/-3 count tolerance, and the generator is still running.
  reinitBus();
  uint32_t after = 0;
  bool readOk = readCount(after);
  uint64_t nowUs = rtcNowUs();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  uint32_t before = countBeforeDeep;
  uint32_t nominal = expectedDeepDelta;
  uint64_t startUs = rtcUsBeforeDeep;
  deepMagic = 0;

  char line[240];
  if (!readOk) {
    snprintf(line, sizeof(line),
             "I2C 0x32 NACK after deep sleep - counter socket may have lost power. FAIL");
  } else if (nowUs < startUs) {
    snprintf(line, sizeof(line),
             "DEEP: clock went backwards across reboot. FAIL");
  } else {
    uint64_t elapsedUs = nowUs - startUs;
    float seconds = (float)elapsedUs / 1000000.0f;
    uint32_t delta = deltaCount(before, after);
    uint32_t expect = (uint32_t)(EXPECTED_HZ * seconds + 0.5f);
    uint32_t err = (delta > expect) ? (delta - expect) : (expect - delta);
    bool rateOk = err <= COUNT_TOLERANCE;
    bool sleptOk = elapsedUs + 500000ULL >= (uint64_t)WINDOW_MS * 1000ULL;
    bool pass = rateOk && sleptOk;
    float hz = (seconds > 0.0f) ? ((float)delta / seconds) : 0.0f;
    snprintf(line, sizeof(line),
             "DEEP %.2fs wake=%s: before=%lu after=%lu delta=%lu expect=%lu nominal=%lu tol=+/-%lu implied=%.2f Hz %s",
             seconds, wakeCauseName(cause), (unsigned long)before, (unsigned long)after,
             (unsigned long)delta, (unsigned long)expect, (unsigned long)nominal,
             (unsigned long)COUNT_TOLERANCE, hz, pass ? "PASS" : "FAIL");
  }
  setHold(line);

  Serial.begin(115200);
  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < 15000) delay(10);
  if (!Serial) return;

  Serial.println();
  Serial.println("=== FED4 PIR Counter (S-35770) ===");
  Serial.println("Woke from deep sleep.");
  Serial.println(line);
  Serial.println("Done. Reset the board to run again.");
}

bool runFresh() {
  Serial.println();
  Serial.println("=== FED4 PIR Counter (S-35770) ===");
  Serial.println("TP1 = COUNT_CLK. Rising edge = +1.");
  Serial.printf("Counting windows expect %.1f Hz for %lu s, tol +/-%lu.\n",
                EXPECTED_HZ, (unsigned long)(WINDOW_MS / 1000),
                (unsigned long)COUNT_TOLERANCE);
  Serial.println("Generator OFF until the countdown. Then leave it ON.");
  Serial.println();

  if (!counterPresent()) {
    const char *line = "I2C 0x32 NACK - counter not found. Check J5 on the main I2C port. FAIL";
    Serial.println(line);
    setHold(line);
    return false;
  }
  Serial.println("I2C 0x32 ACK");

  if (!resetCounter()) {
    const char *line = "Reset write failed. FAIL";
    Serial.println(line);
    setHold(line);
    return false;
  }

  uint32_t count = 0;
  if (!sample("Reset", count)) return false;
  if (count != 0) {
    char line[80];
    snprintf(line, sizeof(line), "Reset -> count %lu FAIL", (unsigned long)count);
    Serial.println(line);
    setHold(line);
    return false;
  }
  Serial.println("Reset -> count 0 PASS");

  Serial.println();
  Serial.printf("QUIET %lu s: generator OFF, TP1 open.\n", (unsigned long)(QUIET_MS / 1000));
  uint32_t before = 0;
  uint32_t after = 0;
  if (!sample("QUIET", before)) return false;
  delay(QUIET_MS);
  if (!sample("QUIET", after)) return false;
  logDelta("QUIET", QUIET_MS / 1000, before, after, 0, 0);

  Serial.println();
  Serial.printf("Start %.1f Hz, 50%% duty, 0-3.0 V square wave on TP1.\n", EXPECTED_HZ);
  Serial.println("Leave it running through deep sleep.");
  for (int sec = 5; sec >= 1; --sec) {
    Serial.println(sec);
    Serial.flush();
    delay(1000);
  }

  Serial.printf("AWAKE %lu s.\n", (unsigned long)(WINDOW_MS / 1000));
  if (!sample("AWAKE", before)) return false;
  delay(WINDOW_MS);
  if (!sample("AWAKE", after)) return false;
  logDelta("AWAKE", WINDOW_MS / 1000, before, after, expectedInWindow(), COUNT_TOLERANCE);

  Serial.println();
  Serial.printf("Entering light sleep (%lu s, timer wake only)...\n",
                (unsigned long)(WINDOW_MS / 1000));
  Serial.flush();
  armTimerWake(WINDOW_MS);
  if (!sample("LIGHT", before)) return false;
  Serial.flush();
  esp_light_sleep_start();

  reinitBus();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (!sample("LIGHT", after)) return false;
  Serial.printf("Light sleep wake: %s (%d)\n", wakeCauseName(cause), (int)cause);
  logDelta("LIGHT", WINDOW_MS / 1000, before, after, expectedInWindow(), COUNT_TOLERANCE);

  Serial.println();
  Serial.printf("Entering deep sleep (%lu s). Serial drops; result prints after reboot.\n",
                (unsigned long)(WINDOW_MS / 1000));
  Serial.flush();
  delay(50);
  armTimerWake(WINDOW_MS);
  if (!sample("DEEP", countBeforeDeep)) return false;
  rtcUsBeforeDeep = rtcNowUs();
  expectedDeepDelta = expectedInWindow();
  deepMagic = DEEP_MAGIC;
  Serial.flush();
  esp_deep_sleep_start();

  deepMagic = 0;
  const char *rejected = "Deep sleep was rejected. FAIL";
  Serial.println(rejected);
  setHold(rejected);
  return false;
}

void setup() {
  if (deepMagic == DEEP_MAGIC) {
    reportDeepWake();
    return;
  }

  Serial.begin(115200);
  reinitBus();
  while (!Serial) delay(10);
  runFresh();
}

void loop() {
  if (!holdResult) return;
  if (!Serial) {
    delay(100);
    return;
  }
  Serial.println(holdLine);
  delay(5000);
}
