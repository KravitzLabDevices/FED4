/*
 * SeeedStudio Sense — TRRS TRIG + UART Submodule (v1.0)
 *
 * Standalone firmware for Seeed XIAO ESP32-S3 Sense. Not part of FED4 library.
 * Spec: ../README.md
 *
 * Pins:
 *   TRIG = GPIO1 (D0) — idle HIGH, active LOW
 *   DATA = GPIO2 (D1) — half-duplex UART 115200 (external pull-up)
 *   SD SPI: CS=GPIO21, SCK=GPIO7, MISO=GPIO8, MOSI=GPIO9
 *
 * Arduino IDE:
 *   Board: Seeed Studio XIAO ESP32S3 Sense
 *   USB CDC On Boot: Enabled
 */

#include <Arduino.h>
#include "esp_sleep.h"
#include "../SubmoduleCommands.h"
#include "../SubmoduleProtocol.h"
#include "../SubmoduleState.h"
#include "../SubmoduleUartEsp32.h"
#include "SenseCamera.h"

static const int PIN_TRIG = 1;
static const int PIN_DATA = 2;
static const uint32_t SERIAL_BOOT_DELAY_MS = 2000;
static const uint32_t SERIAL_WAKE_GRACE_MS = 500;

static SubmoduleState gState;
static SubmoduleUartEsp32 gUart;

// millis() sampled immediately when light sleep returns (before Serial delays).
static uint32_t gWakeMs = 0;

static const char *wakeCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "UNDEFINED";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "GPIO";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "TIMER";
    case ESP_SLEEP_WAKEUP_UART:
      return "UART";
    default:
      return "OTHER";
  }
}

static bool renameAdapter(const char *fromName, const char *toName) {
  return senseRenameCapture(fromName, toName);
}

static bool captureAdapter(SubmoduleState *state) {
  const uint32_t captureStartMs = millis();
  Serial.println("captureAdapter: enter");
  Serial.flush();
  char name[32];
  bool ok = false;

  if (state != nullptr && !state->rtcValid) {
    Serial.println("captureAdapter: RTC invalid — writing invalidrtc.jpeg");
    Serial.flush();
    ok = senseCaptureImageNamed("invalidrtc.jpeg", name, sizeof(name));
  } else {
    ok = senseCaptureImageNow(name, sizeof(name));
  }

  const uint32_t savedMs = millis();
  const uint32_t captureOnlyMs = savedMs - captureStartMs;
  const uint32_t wakeToSavedMs = savedMs - gWakeMs;

  if (!ok) {
    submoduleStateSetError(state, SUBMODULE_ERR_CAPTURE);
    Serial.printf("CAPTURE failed: %s\n", senseCaptureLastError());
    Serial.printf("TIMING: wake→fail %lu ms | capture-only %lu ms\n",
                  (unsigned long)wakeToSavedMs, (unsigned long)captureOnlyMs);
    Serial.flush();
    return false;
  }
  snprintf(state->lastFilename, sizeof(state->lastFilename), "%s", name);
  submoduleStateSetError(state, SUBMODULE_ERR_NONE);
  Serial.printf("CAPTURE saved: %s\n", name);
  Serial.printf("TIMING: wake→saved %lu ms | capture-only %lu ms\n",
                (unsigned long)wakeToSavedMs, (unsigned long)captureOnlyMs);
  Serial.flush();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("SeeedStudioSense TRIG+UART submodule v1.0");
  Serial.printf("TRIG=GPIO%d DATA=GPIO%d baud=%lu\n", PIN_TRIG, PIN_DATA,
                (unsigned long)SUBMODULE_UART_BAUD);
  Serial.println("Manual test: pull TRIG LOW to wake, then release HIGH to exit UART wait");
  Serial.flush();

  submoduleStateInit(&gState);
  submoduleSetRenameFn(renameAdapter);

  const SubmoduleUartEsp32Config cfg = {
      PIN_TRIG,
      PIN_DATA,
      SUBMODULE_UART_BAUD,
  };

  if (!submoduleUartEsp32Init(&gUart, &cfg, &gState)) {
    Serial.println("UART/TRIG init FAILED");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("Init OK — TRIG idle read=%d (expect 1)\n", digitalRead(PIN_TRIG));
  Serial.println("Ready — light sleep until TRIG LOW");
  Serial.flush();
}

void loop() {
  Serial.println("Entering light sleep...");
  Serial.flush();

  submoduleUartEsp32EnterLightSleep(&gUart);
  gWakeMs = millis(); // first instruction after wake — timing baseline

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const int trigAtWake = digitalRead(PIN_TRIG);

  // Capture ASAP (no Serial grace before shutter). Print timing after.
  submoduleUartEsp32HandleWakeSession(&gUart, captureAdapter);

  delay(SERIAL_WAKE_GRACE_MS);
  Serial.printf("Light sleep exited — cause=%s (%d) TRIG@wake=%d\n",
                wakeCauseName(cause), (int)cause, trigAtWake);
  Serial.printf("[post-session] TRIG=%d rtcValid=%d lastErr=%u wakeAge=%lu ms\n",
                digitalRead(PIN_TRIG), (int)gState.rtcValid,
                (unsigned)gState.lastErrorCode,
                (unsigned long)(millis() - gWakeMs));
  Serial.println("HandleWakeSession returned — back to sleep");
  Serial.flush();
}
