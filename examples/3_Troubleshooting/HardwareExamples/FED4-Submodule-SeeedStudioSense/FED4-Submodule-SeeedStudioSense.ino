/*
 * FED4 Submodule Master Test — SeeedStudio Sense (v0.3)
 *
 * Reads submodule status, syncs RTC only when needed, then sends CAPTURE commands.
 * Companion to: examples/Submodules/SeeedStudioSense/SeeedStudioSense.ino
 * Protocol: examples/Submodules/README.md
 *
 * Wiring (shared main I2C bus):
 *   FED4 SDA (GPIO8)  -> Seeed D4 (GPIO5)
 *   FED4 SCL (GPIO9)  -> Seeed D5 (GPIO6)
 *   GND               -> GND
 *   3V3               -> 3V3
 *
 * Hardware (see src/FED4_Pins.h):
 *   SDA=8, SCL=9, 100 kHz
 *   DS3231 RTC @ 0x68 on main bus
 */

#include <Wire.h>
#include <RTClib.h>
#include <FED4_Pins.h>
#include "../../../Submodules/SubmoduleProtocol.h"

static const uint8_t I2C_SUBMODULE_ADDR = SUBMODULE_I2C_ADDR;
static const uint32_t TX_INTERVAL_MS = 15000;
static const uint32_t SERIAL_BOOT_DELAY_MS = 1000;
static const uint32_t READY_POLL_MS = 50;
static const uint32_t READY_TIMEOUT_MS = 2000;
static const uint32_t TX_RETRY_DELAY_MS = 100;
static const uint32_t BETWEEN_COMMANDS_MS = 500;
static const uint32_t CAPTURE_SETTLE_MS = 3000;

RTC_DS3231 rtc;

const char *txErrorStr(uint8_t err) {
  switch (err) {
    case 0:
      return "OK";
    case 1:
      return "data too long";
    case 2:
      return "NAK on address";
    case 3:
      return "NAK on data";
    case 4:
      return "other error";
    case 5:
      return "timeout";
    default:
      return "unknown";
  }
}

uint8_t probeSubmodule() {
  Wire.beginTransmission(I2C_SUBMODULE_ADDR);
  return Wire.endTransmission(true);
}

void recoverI2cBus() {
  Wire.end();
  pinMode(SCL, INPUT_PULLUP);
  pinMode(SDA, INPUT_PULLUP);
  delay(1);

  pinMode(SCL, OUTPUT);
  digitalWrite(SCL, HIGH);
  for (int i = 0; i < 9 && digitalRead(SDA) == LOW; i++) {
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(5);
  }

  pinMode(SCL, INPUT_PULLUP);
  pinMode(SDA, INPUT_PULLUP);
  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);
}

void printHex(const char *label, const uint8_t *data, size_t len) {
  Serial.print(label);
  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      Serial.print(' ');
    }
    Serial.printf("%02X", data[i]);
  }
  Serial.println();
}

void printSubmoduleStatus(const SubmoduleStatus *status) {
  if (status == nullptr) {
    return;
  }

  Serial.printf("  Status: 0x%02X (proto v%u, RTC=%s, SD=%s, busy=%s)\n",
                status->flags,
                (unsigned)submoduleStatusProtocolVersion(status->flags),
                submoduleStatusRtcValid(status->flags) ? "valid" : "unset",
                submoduleStatusSdReady(status->flags) ? "ready" : "not ready",
                submoduleStatusBusy(status->flags) ? "yes" : "no");
  Serial.printf("  Last error: %u (%s)\n", (unsigned)status->lastError,
                submoduleErrorStr(status->lastError));
}

bool sendToSubmodule(const uint8_t *data, size_t len) {
  for (int attempt = 1; attempt <= 2; attempt++) {
    Wire.beginTransmission(I2C_SUBMODULE_ADDR);
    for (size_t i = 0; i < len; i++) {
      Wire.write(data[i]);
    }
    const uint8_t err = Wire.endTransmission(true);
    if (err == 0) {
      Serial.printf("  TX OK (attempt %d)\n", attempt);
      return true;
    }
    Serial.printf("  TX fail attempt %d: %s (%u)\n", attempt, txErrorStr(err), err);
    if (err == 4) {
      recoverI2cBus();
    }
    if (attempt == 1) {
      delay(TX_RETRY_DELAY_MS);
    }
  }
  return false;
}

bool wakeAndWaitForSubmodule() {
  const uint8_t wakeErr = probeSubmodule();
  Serial.printf("  Wake ping: %s (%u)\n", txErrorStr(wakeErr), wakeErr);
  if (wakeErr == 4) {
    recoverI2cBus();
  }

  Serial.println("  Polling for submodule ACK...");
  const unsigned long deadline = millis() + READY_TIMEOUT_MS;
  unsigned long waited = 0;

  while (millis() < deadline) {
    delay(READY_POLL_MS);
    waited += READY_POLL_MS;

    const uint8_t err = probeSubmodule();
    if (err == 0) {
      Serial.printf("  Submodule ready (%lu ms after wake ping)\n", waited);
      return true;
    }
    if (err == 4) {
      recoverI2cBus();
    }
  }

  Serial.printf("  Submodule not ready after %lu ms\n", (unsigned long)READY_TIMEOUT_MS);
  return false;
}

bool readSubmoduleStatus(SubmoduleStatus *status) {
  if (status == nullptr) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(I2C_SUBMODULE_ADDR,
                                            (uint8_t)SUBMODULE_STATUS_READ_LEN);
  if (received != SUBMODULE_STATUS_READ_LEN) {
    Serial.printf("  Status read failed — got %u of %u bytes\n",
                  (unsigned)received, (unsigned)SUBMODULE_STATUS_READ_LEN);
    return false;
  }

  status->flags = (uint8_t)Wire.read();
  status->lastError = (uint8_t)Wire.read();
  printSubmoduleStatus(status);
  return true;
}

bool wakeReadStatus(SubmoduleStatus *status) {
  if (!wakeAndWaitForSubmodule()) {
    return false;
  }
  return readSubmoduleStatus(status);
}

bool readRtc(DateTime *out) {
  if (out == nullptr) {
    return false;
  }
  *out = rtc.now();
  return true;
}

bool syncSubmoduleTimeIfNeeded() {
  Serial.println();
  Serial.println("=== Sync RTC (if needed) ===");

  SubmoduleStatus status = {};
  if (!wakeReadStatus(&status)) {
    Serial.println("  Submodule not ready — sync skipped.");
    return false;
  }

  if (submoduleStatusRtcValid(status.flags)) {
    Serial.println("  RTC already valid — skipping SET_TIME");
    return true;
  }

  DateTime now;
  if (!readRtc(&now)) {
    now = DateTime(F(__DATE__), F(__TIME__));
  }

  uint8_t frame[SUBMODULE_SET_TIME_FRAME_LEN];
  const size_t len = submodulePackSetTime(
      frame, (uint16_t)now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());
  printHex("  Payload: ", frame, len);

  if (!sendToSubmodule(frame, len)) {
    Serial.println("  SET_TIME failed.");
    return false;
  }
  return true;
}

bool sendCaptureCommand(const char *name, const uint8_t *data, size_t len) {
  Serial.println();
  Serial.print("=== ");
  Serial.println(name);
  printHex("  Payload: ", data, len);

  SubmoduleStatus status = {};
  if (!wakeReadStatus(&status)) {
    Serial.println("  Submodule not ready — command skipped.");
    return false;
  }

  if (!sendToSubmodule(data, len)) {
    Serial.println("  Submodule did not ACK.");
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  Serial.println("=== FED4 Submodule Master Test (v0.3) ===");
  Serial.println("Target: SeeedStudio Sense @ 0x42");
  Serial.println("Flow: wake -> read status -> SET_TIME if RTC unset -> CAPTURE");
  Serial.println("Capture diagnostics are on the Seeed USB Serial.");
  Serial.println();

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);
  Serial.println("I2C: SDA=8, SCL=9, 100kHz");

  if (!rtc.begin(&Wire)) {
    Serial.println("WARNING: DS3231 RTC not found @ 0x68 — SET_TIME will use compile time.");
  } else {
    Serial.println("RTC: DS3231 @ 0x68 OK");
  }
  Serial.println();
}

void loop() {
  static uint16_t captureId = 1;
  uint8_t frame[SUBMODULE_SET_TIME_FRAME_LEN];

  Serial.println("=== Test cycle ===");

  if (syncSubmoduleTimeIfNeeded()) {
    delay(BETWEEN_COMMANDS_MS);
  }

  const size_t captureDatetimeLen = submodulePackCaptureDatetime(frame);
  if (sendCaptureCommand("CAPTURE datetime", frame, captureDatetimeLen)) {
    Serial.printf("  Waiting %lu ms for capture + sleep...\n",
                  (unsigned long)CAPTURE_SETTLE_MS);
    delay(CAPTURE_SETTLE_MS);
  }

  const size_t captureIdLen = submodulePackCaptureWithId(frame, captureId++);
  if (sendCaptureCommand("CAPTURE with ID", frame, captureIdLen)) {
    Serial.printf("  Waiting %lu ms for capture + sleep...\n",
                  (unsigned long)CAPTURE_SETTLE_MS);
    delay(CAPTURE_SETTLE_MS);
  }

  Serial.println();
  Serial.printf("Next cycle in %lu s\n", (unsigned long)(TX_INTERVAL_MS / 1000));
  delay(TX_INTERVAL_MS);
}
