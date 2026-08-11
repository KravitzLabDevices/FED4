/*
 * SeeedStudio Sense — I2C Slave Submodule (v0.3)
 *
 * Standalone firmware for Seeed XIAO ESP32-S3 Sense. Not part of FED4 library.
 * Spec: ../README.md
 *
 * Light-sleeps until SCL activity; master wakes with START + 0x42 + W.
 * Companion master test: FED4-Submodule-SeeedStudioSense
 *
 * Pins (XIAO ESP32-S3 Sense):
 *   D4 / SDA = GPIO5
 *   D5 / SCL = GPIO6
 *   SD SPI: CS=GPIO21, SCK=GPIO7, MISO=GPIO8, MOSI=GPIO9 (expansion board)
 *   GPIO21 is SD CS — USER_LED feedback disabled (shared pin)
 *
 * Arduino IDE:
 *   Board: Seeed Studio XIAO ESP32S3 Sense
 *   USB CDC On Boot: Enabled
 *   Insert microSD card before capture tests; heat sink recommended for camera use.
 */

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "../SubmoduleProtocol.h"
#include "SenseCamera.h"

static const uint8_t I2C_SLAVE_ADDR = SUBMODULE_I2C_ADDR;
static const uint32_t I2C_FREQ = 100000;

static const uint32_t SERIAL_BOOT_DELAY_MS = 2000;
static const uint32_t LISTEN_MS = 9000;
static const uint32_t POST_WAKE_SETTLE_MS = 20;
static const uint32_t BUS_IDLE_TIMEOUT_US = 50000;
static const int I2C_INIT_RETRIES = 15;

static const size_t RX_BUF_SIZE = 16;

static uint8_t rxBuf[RX_BUF_SIZE];
static volatile size_t rxLen = 0;
static volatile bool rxPending = false;
static bool i2cSlaveActive = false;

static bool rtcValid = false;
static SenseDateTime rtcTime = {};
static bool sdReady = false;
static uint8_t lastErrorCode = SUBMODULE_ERR_NONE;
static bool lastCaptureSucceeded = false;

void shutdownI2cForSleep();

uint8_t buildStatusFlags() {
  uint8_t flags = 0;
  if (rtcValid) {
    flags |= SUBMODULE_STATUS_RTC_VALID;
  }
  if (sdReady) {
    flags |= SUBMODULE_STATUS_SD_READY;
  }
  return flags;
}

void setLastError(uint8_t code) {
  lastErrorCode = code;
}

void mapCaptureErrorFromMessage(const char *err) {
  if (err == nullptr || err[0] == '\0') {
    setLastError(SUBMODULE_ERR_UNKNOWN);
    return;
  }
  if (strstr(err, "SD mount failed") != nullptr) {
    setLastError(SUBMODULE_ERR_SD_MOUNT);
  } else if (strstr(err, "SD card not detected") != nullptr ||
             strstr(err, "SD card unreadable") != nullptr) {
    setLastError(SUBMODULE_ERR_SD_NOT_DETECTED);
  } else if (strstr(err, "SD write") != nullptr ||
             strstr(err, "SD file open failed") != nullptr) {
    setLastError(SUBMODULE_ERR_SD_WRITE);
  } else if (strstr(err, "camera init failed") != nullptr) {
    setLastError(SUBMODULE_ERR_CAMERA_INIT);
  } else if (strstr(err, "camera frame capture failed") != nullptr) {
    setLastError(SUBMODULE_ERR_CAPTURE_FRAME);
  } else {
    setLastError(SUBMODULE_ERR_UNKNOWN);
  }
}

void onRequest() {
  Wire.write(submoduleBuildStatusByte(buildStatusFlags()));
  Wire.write(lastErrorCode);
}

void onReceive(int byteCount) {
  rxLen = 0;
  while (Wire.available() && rxLen < RX_BUF_SIZE) {
    rxBuf[rxLen++] = (uint8_t)Wire.read();
  }
  while (Wire.available()) {
    Wire.read();
  }
  rxPending = true;
  (void)byteCount;
}

void clearRx() {
  rxLen = 0;
  rxPending = false;
}

bool validateDateTime(uint16_t year, uint8_t month, uint8_t day,
                      uint8_t hour, uint8_t min, uint8_t sec) {
  if (year < 2000 || year > 2099) {
    return false;
  }
  if (month < 1 || month > 12) {
    return false;
  }
  if (day < 1 || day > 31) {
    return false;
  }
  if (hour > 23 || min > 59 || sec > 59) {
    return false;
  }
  return true;
}

void logCaptureResult(const char *label, bool ok) {
  if (ok) {
    Serial.printf("%s: OK\n", label);
    return;
  }

  const char *err = senseCaptureLastError();
  if (err != nullptr && err[0] != '\0') {
    Serial.printf("%s: failed (%s)\n", label, err);
  } else {
    Serial.printf("%s: failed\n", label);
  }
}

bool applySetTime(const uint8_t *frame, size_t len) {
  if (len != SUBMODULE_SET_TIME_FRAME_LEN || frame[0] != SUBMODULE_CMD_SET_TIME) {
    Serial.printf("SET_TIME: ignored — expected %u bytes, got %u\n",
                  (unsigned)SUBMODULE_SET_TIME_FRAME_LEN, (unsigned)len);
    return false;
  }

  const uint16_t year = submoduleReadU16Le(&frame[1]);
  const uint8_t month = frame[3];
  const uint8_t day = frame[4];
  const uint8_t hour = frame[5];
  const uint8_t min = frame[6];
  const uint8_t sec = frame[7];

  if (!validateDateTime(year, month, day, hour, min, sec)) {
    Serial.println("SET_TIME: invalid datetime — ignored");
    return false;
  }

  struct tm tmTime = {};
  tmTime.tm_year = (int)year - 1900;
  tmTime.tm_mon = (int)month - 1;
  tmTime.tm_mday = (int)day;
  tmTime.tm_hour = (int)hour;
  tmTime.tm_min = (int)min;
  tmTime.tm_sec = (int)sec;
  tmTime.tm_isdst = -1;

  struct timeval tv = {};
  tv.tv_sec = mktime(&tmTime);
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) {
    Serial.println("SET_TIME: settimeofday failed");
    return false;
  }

  rtcTime.year = year;
  rtcTime.month = month;
  rtcTime.day = day;
  rtcTime.hour = hour;
  rtcTime.min = min;
  rtcTime.sec = sec;
  rtcValid = true;

  Serial.printf("SET_TIME: %04u-%02u-%02u %02u:%02u:%02u (rtcValid=true)\n",
                (unsigned)year, (unsigned)month, (unsigned)day,
                (unsigned)hour, (unsigned)min, (unsigned)sec);
  return true;
}

bool applyCaptureImage(const uint8_t *frame, size_t len) {
  if (len < SUBMODULE_CAPTURE_MIN_LEN || frame[0] != SUBMODULE_CMD_CAPTURE_IMAGE) {
    return false;
  }

  if (len == SUBMODULE_CAPTURE_MIN_LEN) {
    if (!rtcValid) {
      setLastError(SUBMODULE_ERR_RTC_NOT_SET);
      Serial.println("CAPTURE datetime: failed (RTC not set — send SET_TIME first)");
      return false;
    }

    shutdownI2cForSleep();
    Serial.println("CAPTURE datetime: starting (I2C released for camera/SD)");
    const bool ok = senseCaptureImageDatetime(&rtcTime);
    if (ok) {
      setLastError(SUBMODULE_ERR_NONE);
      sdReady = true;
    } else {
      mapCaptureErrorFromMessage(senseCaptureLastError());
    }
    logCaptureResult("CAPTURE datetime", ok);
    return ok;
  }

  if (len == SUBMODULE_CAPTURE_WITH_ID_LEN) {
    const uint16_t imageId = submoduleReadU16Le(&frame[1]);

    shutdownI2cForSleep();
    Serial.printf("CAPTURE id %05u: starting (I2C released for camera/SD)\n",
                  (unsigned)imageId);
    const bool ok = senseCaptureImageById(imageId);
    if (ok) {
      setLastError(SUBMODULE_ERR_NONE);
      sdReady = true;
    } else {
      mapCaptureErrorFromMessage(senseCaptureLastError());
    }
    char label[32];
    snprintf(label, sizeof(label), "CAPTURE id %05u", (unsigned)imageId);
    logCaptureResult(label, ok);
    return ok;
  }

  return false;
}

bool dispatchCommand() {
  if (rxLen == 0) {
    return false;
  }

  const uint8_t cmd = rxBuf[0];
  Serial.printf("CMD 0x%02X (%u bytes)\n", cmd, (unsigned)rxLen);

  if (cmd == SUBMODULE_CMD_RESERVED) {
    return false;
  }

  if (cmd == SUBMODULE_CMD_SET_TIME) {
    applySetTime(rxBuf, rxLen);
    return false;
  }

  if (cmd == SUBMODULE_CMD_CAPTURE_IMAGE) {
    if (rxLen == SUBMODULE_CAPTURE_MIN_LEN ||
        rxLen == SUBMODULE_CAPTURE_WITH_ID_LEN) {
      lastCaptureSucceeded = applyCaptureImage(rxBuf, rxLen);
      return true;
    }
    Serial.printf("CAPTURE: ignored — expected 1 or 3 bytes, got %u\n",
                  (unsigned)rxLen);
    return false;
  }

  Serial.printf("Unknown command 0x%02X (%u bytes) — ignored\n",
                cmd, (unsigned)rxLen);
  return false;
}

void shutdownI2cForSleep() {
  if (i2cSlaveActive) {
    Wire.end();
    i2cSlaveActive = false;
  }
}

bool initI2cSlave() {
  shutdownI2cForSleep();

  gpio_reset_pin((gpio_num_t)SCL);
  gpio_reset_pin((gpio_num_t)SDA);

  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  i2cSlaveActive = Wire.begin((uint8_t)I2C_SLAVE_ADDR, SDA, SCL, I2C_FREQ);
  return i2cSlaveActive;
}

void configureSleepBusPins() {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << SCL) | (1ULL << SDA);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
}

bool prepareBusAfterWake() {
  gpio_reset_pin((gpio_num_t)SCL);
  gpio_reset_pin((gpio_num_t)SDA);

  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);

  const unsigned long start = micros();
  while ((digitalRead(SCL) == LOW || digitalRead(SDA) == LOW) &&
         (unsigned long)(micros() - start) < BUS_IDLE_TIMEOUT_US) {
    delayMicroseconds(100);
  }

  if (digitalRead(SCL) == HIGH && digitalRead(SDA) == HIGH) {
    return true;
  }

  pinMode(SDA, INPUT);
  pinMode(SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(SCL, HIGH);

  for (int i = 0; i < 9 && digitalRead(SDA) == LOW; i++) {
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
  }

  pinMode(SCL, INPUT);
  pinMode(SDA, INPUT);
  return digitalRead(SCL) == HIGH && digitalRead(SDA) == HIGH;
}

bool startupI2cSlaveAfterWake() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

  delay(POST_WAKE_SETTLE_MS);
  prepareBusAfterWake();

  for (int attempt = 1; attempt <= I2C_INIT_RETRIES; attempt++) {
    if (initI2cSlave()) {
      return true;
    }
    delay(20);
  }

  Serial.printf("I2C slave init FAILED — SDA=%d SCL=%d\n", digitalRead(SDA), digitalRead(SCL));
  return false;
}

void enterLightSleep() {
  shutdownI2cForSleep();
  configureSleepBusPins();

  gpio_wakeup_enable((gpio_num_t)SCL, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  Serial.println("Sleeping (SCL wake)...");
  Serial.flush();
  esp_light_sleep_start();
}

const char *wakeupCauseStr() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_GPIO:
      return "GPIO (SCL)";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "undefined (boot)";
    default:
      return "other";
  }
}

void handleWake() {
  const unsigned long wakeMs = millis();

  if (!startupI2cSlaveAfterWake()) {
    Serial.printf("Wake: %s — I2C slave init FAILED\n", wakeupCauseStr());
    return;
  }

  Serial.printf("Wake: %s — I2C slave ready @ 0x%02X (+%lu ms, rtcValid=%s)\n",
                wakeupCauseStr(), I2C_SLAVE_ADDR, millis() - wakeMs,
                rtcValid ? "true" : "false");

  const unsigned long listenUntil = millis() + LISTEN_MS;
  bool sleepAfterCapture = false;

  while (millis() < listenUntil && !sleepAfterCapture) {
    if (rxPending) {
      sleepAfterCapture = dispatchCommand();
      clearRx();
    }
    delay(5);
  }

  if (sleepAfterCapture) {
    if (lastCaptureSucceeded) {
      Serial.println("CAPTURE complete — returning to sleep");
    } else {
      const char *err = senseCaptureLastError();
      if (err != nullptr && err[0] != '\0') {
        Serial.printf("CAPTURE failed (%s) — returning to sleep\n", err);
      } else {
        Serial.println("CAPTURE failed — returning to sleep");
      }
    }
    Serial.flush();
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  Serial.println();
  Serial.println("SeeedStudio Sense I2C slave submodule v0.3");
  Serial.printf("Address: 0x%02X  SDA: D4/GPIO%d  SCL: D5/GPIO%d  %lu kHz\n",
                I2C_SLAVE_ADDR, SDA, SCL, (unsigned long)(I2C_FREQ / 1000));
  Serial.println("Protocol v0.3: status read + 0x01 SET_TIME, 0x02 CAPTURE_IMAGE");
  Serial.println("Light sleep: SCL low-level wake (ESP32-S3)");

  sdReady = senseSdCardReady();
  if (sdReady) {
    Serial.println("SD card: detected at boot");
  } else {
    Serial.println("SD card: NOT detected at boot — check card + J3 pad on expansion");
  }
  Serial.println("Capture diagnostics print on this Serial port (not FED4).");

  if (!initI2cSlave()) {
    Serial.println("I2C slave init FAILED at boot");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("I2C slave ready at boot — entering sleep loop");
}

void loop() {
  enterLightSleep();
  handleWake();
}
