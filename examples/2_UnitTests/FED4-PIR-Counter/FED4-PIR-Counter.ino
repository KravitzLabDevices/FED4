/*
 * FED4 PIR Counter Test (S-35770E01)
 *
 * Standalone bring-up for the counter addon. Does not call FED4::sleep()
 * or startSleep(). Light sleep here is esp_light_sleep_start(), the same
 * ESP-IDF call the idle path uses.
 *
 * J5 (SCL, SDA, +3.0 V, GND) plugs into a main I2C port. Bus is SDA GPIO 8,
 * SCL GPIO 9, always on. Address 0x32. This sketch does not switch PSV2
 * or PSV3.
 *
 * COUNT_CLK (TP1) is CLKIN. Each rising edge counts +1, so 10 Hz is 10
 * counts per second, not 20. R23 (100k) holds CLKIN low when TP1 is open.
 * Drive TP1 from 0 V to about 3.0 V. Do not exceed 5.5 V.
 *
 * Suggested bench wave: 10 Hz, 50% duty, 0-3.0 V. Leave the generator off
 * for the quiet window, then on through awake, light sleep, and deep sleep.
 * The sketch only prints counts. You decide whether the numbers are right.
 *
 * RST is pulled up on the board. The sketch resets the count over I2C.
 * A count here does not mean the PIR output is wired yet.
 *
 * Serial 115200. Open the monitor before reset. Deep sleep reboots the
 * ESP32; the deep-sleep counts print when the monitor reconnects.
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <FED4_Pins.h>
#include <sys/time.h>

static const uint8_t COUNTER_ADDR = 0x32;
static const uint32_t WINDOW_MS = 10000;
static const uint32_t QUIET_MS = 5000;
static const uint32_t COUNTER_MOD = 0x01000000u;
static const uint32_t DEEP_MAGIC = 0x53353770u;

static const uint8_t RESET_POINTER = 0x81; // B7 address pointer, B0 test bit
static const uint8_t RESET_RST = 0x02;     // RST2:0 = 010

RTC_DATA_ATTR uint32_t deepMagic = 0;
RTC_DATA_ATTR uint32_t countBeforeDeep = 0;
RTC_DATA_ATTR uint64_t rtcUsBeforeDeep = 0;

static bool showLiveCount = false;

uint64_t rtcNowUs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
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

void printWindow(const char *name, float seconds, uint32_t before, uint32_t after) {
  Serial.printf("%s %.2fs: before=%lu after=%lu delta=%lu counter=%lu\n",
                name, seconds, (unsigned long)before, (unsigned long)after,
                (unsigned long)deltaCount(before, after), (unsigned long)after);
}

bool sample(const char *what, uint32_t &count) {
  if (readCount(count)) return true;
  Serial.printf("%s: I2C read failed\n", what);
  return false;
}

void armTimerWake(uint32_t ms) {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
}

void reportDeepWake() {
  // Sample before Serial.begin. USB reconnect can take a while, and the
  // generator is still running.
  reinitBus();
  uint32_t after = 0;
  bool readOk = readCount(after);
  uint64_t nowUs = rtcNowUs();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  uint32_t before = countBeforeDeep;
  uint64_t startUs = rtcUsBeforeDeep;
  deepMagic = 0;

  Serial.begin(115200);
  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < 15000) delay(10);

  Serial.println();
  Serial.println("=== FED4 PIR Counter (S-35770) ===");
  Serial.println("Woke from deep sleep.");
  if (!readOk) {
    Serial.println("I2C 0x32 NACK after deep sleep - counter socket may have lost power.");
  } else if (nowUs < startUs) {
    Serial.printf("DEEP wake=%s: before=%lu after=%lu delta=%lu counter=%lu\n",
                  wakeCauseName(cause), (unsigned long)before, (unsigned long)after,
                  (unsigned long)deltaCount(before, after), (unsigned long)after);
  } else {
    float seconds = (float)(nowUs - startUs) / 1000000.0f;
    Serial.printf("DEEP %.2fs wake=%s: before=%lu after=%lu delta=%lu counter=%lu\n",
                  seconds, wakeCauseName(cause), (unsigned long)before, (unsigned long)after,
                  (unsigned long)deltaCount(before, after), (unsigned long)after);
  }
  Serial.println("Counting live. Reset the board to run the windows again.");
  showLiveCount = true;
}

void runFresh() {
  Serial.println();
  Serial.println("=== FED4 PIR Counter (S-35770) ===");
  Serial.println("TP1 = COUNT_CLK. Rising edge = +1.");
  Serial.println("Generator off for the quiet window, then on through deep sleep.");
  Serial.println();

  if (!counterPresent()) {
    Serial.println("I2C 0x32 NACK - counter not found. Check J5 on the main I2C port.");
    showLiveCount = true;
    return;
  }
  Serial.println("I2C 0x32 ACK");

  uint32_t count = 0;
  if (!resetCounter()) {
    Serial.println("Reset write failed.");
  } else if (!sample("Reset", count)) {
    // sample already printed
  } else {
    Serial.printf("Reset: counter=%lu\n", (unsigned long)count);
  }

  Serial.println();
  Serial.printf("QUIET %lu s.\n", (unsigned long)(QUIET_MS / 1000));
  uint32_t before = 0;
  uint32_t after = 0;
  if (sample("QUIET", before)) {
    delay(QUIET_MS);
    if (sample("QUIET", after)) {
      printWindow("QUIET", QUIET_MS / 1000.0f, before, after);
    }
  }

  Serial.println();
  Serial.println("Start the square wave on TP1. Leave it running through deep sleep.");
  for (int sec = 5; sec >= 1; --sec) {
    Serial.println(sec);
    Serial.flush();
    delay(1000);
  }

  Serial.printf("AWAKE %lu s.\n", (unsigned long)(WINDOW_MS / 1000));
  if (sample("AWAKE", before)) {
    delay(WINDOW_MS);
    if (sample("AWAKE", after)) {
      printWindow("AWAKE", WINDOW_MS / 1000.0f, before, after);
    }
  }

  Serial.println();
  Serial.printf("Entering light sleep (%lu s, timer wake only)...\n",
                (unsigned long)(WINDOW_MS / 1000));
  Serial.flush();
  armTimerWake(WINDOW_MS);
  bool lightBeforeOk = sample("LIGHT", before);
  Serial.flush();
  esp_light_sleep_start();

  reinitBus();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("Light sleep wake: %s (%d)\n", wakeCauseName(cause), (int)cause);
  if (lightBeforeOk && sample("LIGHT", after)) {
    printWindow("LIGHT", WINDOW_MS / 1000.0f, before, after);
  }

  Serial.println();
  Serial.printf("Entering deep sleep (%lu s). Serial drops; counts print after reboot.\n",
                (unsigned long)(WINDOW_MS / 1000));
  Serial.flush();
  delay(50);
  armTimerWake(WINDOW_MS);
  if (!sample("DEEP", countBeforeDeep)) {
    showLiveCount = true;
    return;
  }
  rtcUsBeforeDeep = rtcNowUs();
  deepMagic = DEEP_MAGIC;
  Serial.flush();
  esp_deep_sleep_start();

  deepMagic = 0;
  Serial.println("Deep sleep was rejected.");
  showLiveCount = true;
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
  if (!showLiveCount) return;
  uint32_t count = 0;
  if (readCount(count)) {
    Serial.printf("counter=%lu\n", (unsigned long)count);
  } else {
    Serial.println("counter=?");
  }
  delay(1000);
}
