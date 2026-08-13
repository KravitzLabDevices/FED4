/*
 * FED4 Submodule Master Test — SeeedStudio Sense (v0.3)
 *
 * Requires FED4_ENABLE_SUBMODULE 1 in src/FED4.h (library rebuild).
 * Companion slave: examples/3_Submodules/SeeedStudioSense/SeeedStudioSense.ino
 *
 * Flow each cycle: wake → status → SET_TIME from DS3231 → CAPTURE datetime
 */

#include <FED4.h>

#if !FED4_ENABLE_SUBMODULE
#error "Set FED4_ENABLE_SUBMODULE to 1 in src/FED4.h and rebuild the library."
#endif

static const uint32_t TX_INTERVAL_MS = 15000;

FED4 fed4;

void printSubmoduleStatus(const SubmoduleStatus *status)
{
  if (status == nullptr)
  {
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

void setup()
{
  // Full board bring-up (Wire, MCP, PSV2, DS3231, …) — Sense talks on same I2C
  fed4.begin("SenseMasterTest");
  Serial.println("=== FED4 Submodule Master Test (library sense*) ===");
  Serial.println("Flow: senseWakeReadStatus → senseSyncAndCapture");
  Serial.println();
}

void loop()
{
  Serial.println("=== Test cycle ===");

  SubmoduleStatus status = {};
  if (fed4.senseWakeReadStatus(&status))
  {
    printSubmoduleStatus(&status);
  }
  else
  {
    Serial.println("  Sense: wake/status failed");
  }

  if (fed4.senseSyncAndCapture())
  {
    Serial.println("  senseSyncAndCapture OK");
  }
  else
  {
    Serial.println("  senseSyncAndCapture failed (Sense absent or NAK)");
  }

  Serial.println();
  Serial.printf("Next cycle in %lu s\n", (unsigned long)(TX_INTERVAL_MS / 1000));
  delay(TX_INTERVAL_MS);
}
