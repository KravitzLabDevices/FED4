/*
 * FED4 Submodule Master Test — SeeedStudio Sense (v0.3)
 *
 * Uses shared SubmoduleMaster helpers from examples/Submodules/.
 * Companion: examples/Submodules/SeeedStudioSense/SeeedStudioSense.ino
 */

#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>
#include "../../../Submodules/SubmoduleMaster.h"
#include "../../../Submodules/SubmoduleProtocol.h"

static const uint32_t TX_INTERVAL_MS = 15000;
static const uint32_t SERIAL_BOOT_DELAY_MS = 1000;
static const uint32_t BETWEEN_COMMANDS_MS = 500;
static const uint32_t CAPTURE_SETTLE_MS = 3000;

static const SubmoduleMasterConfig kMasterConfig = {
    SUBMODULE_I2C_ADDR,
    SDA,
    SCL,
    100000,
    50,
    2000,
    100,
};

RTC_DS3231 rtc;
Adafruit_MCP23X17 mcp;

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

bool syncSubmoduleTime() {
  Serial.println();
  Serial.println("=== Sync RTC ===");

  SubmoduleStatus status = {};
  if (!submoduleMasterWakeReadStatus(&kMasterConfig, &status)) {
    Serial.println("  Submodule not ready — sync skipped.");
    return false;
  }
  printSubmoduleStatus(&status);

  DateTime now = rtc.now();
  Serial.printf("  FED4 DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());

  uint8_t frame[SUBMODULE_SET_TIME_FRAME_LEN];
  const size_t len = submodulePackSetTime(
      frame, (uint16_t)now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());
  printHex("  Payload: ", frame, len);

  if (!submoduleMasterSend(&kMasterConfig, frame, len)) {
    Serial.println("  SET_TIME failed.");
    return false;
  }
  Serial.println("  SET_TIME OK");
  return true;
}

bool sendCaptureCommand(const char *name, const uint8_t *data, size_t len) {
  Serial.println();
  Serial.print("=== ");
  Serial.println(name);
  printHex("  Payload: ", data, len);

  SubmoduleStatus status = {};
  if (!submoduleMasterWakeReadStatus(&kMasterConfig, &status)) {
    Serial.println("  Submodule not ready — command skipped.");
    return false;
  }
  printSubmoduleStatus(&status);

  if (!submoduleMasterSend(&kMasterConfig, data, len)) {
    Serial.println("  Submodule did not ACK.");
    return false;
  }
  Serial.println("  TX OK");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  Serial.println("=== FED4 Submodule Master Test (v0.3) ===");
  Serial.println("Shared module: SubmoduleMaster");
  Serial.println("Flow: wake -> read status -> SET_TIME from DS3231 -> CAPTURE datetime");
  Serial.println();

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  if (!mcp.begin_I2C()) {
    Serial.println("WARNING: MCP23017 not found @ 0x20 — RTC may be unpowered");
  } else {
    mcp.pinMode(EXP_PSV2_EN, OUTPUT);
    mcp.digitalWrite(EXP_PSV2_EN, LOW); // PSV2 ~ON (active-low): RTC rail
    delay(5);
  }

  if (!rtc.begin(&Wire)) {
    Serial.println("WARNING: DS3231 RTC not found @ 0x68");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power — setting compile time on DS3231");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime now = rtc.now();
    Serial.printf("RTC: DS3231 @ 0x68 OK — %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
  }
  Serial.println();
}

void loop() {
  uint8_t frame[SUBMODULE_SET_TIME_FRAME_LEN];

  Serial.println("=== Test cycle ===");

  if (syncSubmoduleTime()) {
    delay(BETWEEN_COMMANDS_MS);
  }

  const size_t captureLen = submodulePackCaptureDatetime(frame);
  if (sendCaptureCommand("CAPTURE datetime", frame, captureLen)) {
    Serial.printf("  Waiting %lu ms for capture + sleep...\n",
                  (unsigned long)CAPTURE_SETTLE_MS);
    delay(CAPTURE_SETTLE_MS);
  }

  Serial.println();
  Serial.printf("Next cycle in %lu s\n", (unsigned long)(TX_INTERVAL_MS / 1000));
  delay(TX_INTERVAL_MS);
}
