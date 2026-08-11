#include "SubmoduleCommands.h"

#include <Arduino.h>
#include "SubmoduleProtocol.h"
#include "SubmoduleRtc.h"

static void logCaptureResult(const char *label, bool ok, const char *errMsg) {
  if (ok) {
    Serial.printf("%s: OK\n", label);
    return;
  }
  if (errMsg != nullptr && errMsg[0] != '\0') {
    Serial.printf("%s: failed (%s)\n", label, errMsg);
  } else {
    Serial.printf("%s: failed\n", label);
  }
}

static bool applyCaptureImage(SubmoduleState *state, const SubmoduleCaptureOps *ops,
                              const uint8_t *frame, size_t len) {
  if (state == nullptr || ops == nullptr || frame == nullptr) {
    return false;
  }
  if (len < SUBMODULE_CAPTURE_MIN_LEN || frame[0] != SUBMODULE_CMD_CAPTURE_IMAGE) {
    return false;
  }

  if (ops->releaseBus != nullptr) {
    ops->releaseBus();
  }

  if (len == SUBMODULE_CAPTURE_MIN_LEN) {
    if (!state->rtcValid) {
      submoduleStateSetError(state, SUBMODULE_ERR_RTC_NOT_SET);
      Serial.println("CAPTURE datetime: failed (RTC not set — send SET_TIME first)");
      return false;
    }
    if (ops->captureDatetime == nullptr) {
      return false;
    }

    SubmoduleDateTime now = {};
    if (!submoduleGetCurrentDateTime(&now)) {
      submoduleStateSetError(state, SUBMODULE_ERR_RTC_NOT_SET);
      Serial.println("CAPTURE datetime: failed (system clock unreadable)");
      return false;
    }

    Serial.printf("CAPTURE datetime: starting @ %04u-%02u-%02u %02u:%02u:%02u "
                  "(I2C released for camera/SD)\n",
                  (unsigned)now.year, (unsigned)now.month, (unsigned)now.day,
                  (unsigned)now.hour, (unsigned)now.min, (unsigned)now.sec);
    const bool ok = ops->captureDatetime(&now);
    const char *err = ops->captureLastError != nullptr ? ops->captureLastError() : nullptr;
    if (ok) {
      submoduleStateSetError(state, SUBMODULE_ERR_NONE);
      state->sdReady = true;
    } else if (err != nullptr) {
      submoduleStateMapCaptureError(state, err);
    }
    logCaptureResult("CAPTURE datetime", ok, err);
    return ok;
  }

  if (len == SUBMODULE_CAPTURE_WITH_ID_LEN) {
    if (ops->captureById == nullptr) {
      return false;
    }

    const uint16_t imageId = submoduleReadU16Le(&frame[1]);
    Serial.printf("CAPTURE id %05u: starting (I2C released for camera/SD)\n",
                  (unsigned)imageId);
    const bool ok = ops->captureById(imageId);
    const char *err = ops->captureLastError != nullptr ? ops->captureLastError() : nullptr;
    if (ok) {
      submoduleStateSetError(state, SUBMODULE_ERR_NONE);
      state->sdReady = true;
    } else if (err != nullptr) {
      submoduleStateMapCaptureError(state, err);
    }
    char label[32];
    snprintf(label, sizeof(label), "CAPTURE id %05u", (unsigned)imageId);
    logCaptureResult(label, ok, err);
    return ok;
  }

  return false;
}

SubmoduleCommandResult submoduleDispatchCommand(SubmoduleState *state,
                                                const SubmoduleCaptureOps *ops,
                                                const uint8_t *frame,
                                                size_t len) {
  SubmoduleCommandResult result = {};

  if (state == nullptr || frame == nullptr || len == 0) {
    return result;
  }

  const uint8_t cmd = frame[0];
  Serial.printf("CMD 0x%02X (%u bytes)\n", cmd, (unsigned)len);

  if (cmd == SUBMODULE_CMD_RESERVED) {
    return result;
  }

  if (cmd == SUBMODULE_CMD_SET_TIME) {
    if (len != SUBMODULE_SET_TIME_FRAME_LEN) {
      Serial.printf("SET_TIME: ignored — expected %u bytes, got %u\n",
                    (unsigned)SUBMODULE_SET_TIME_FRAME_LEN, (unsigned)len);
      return result;
    }
    if (submoduleApplySetTimeFrame(&state->rtcTime, &state->rtcValid, frame, len)) {
      Serial.printf("SET_TIME: %04u-%02u-%02u %02u:%02u:%02u (rtcValid=true)\n",
                    (unsigned)state->rtcTime.year, (unsigned)state->rtcTime.month,
                    (unsigned)state->rtcTime.day, (unsigned)state->rtcTime.hour,
                    (unsigned)state->rtcTime.min, (unsigned)state->rtcTime.sec);
    } else {
      Serial.println("SET_TIME: invalid datetime or settimeofday failed — ignored");
    }
    return result;
  }

  if (cmd == SUBMODULE_CMD_CAPTURE_IMAGE) {
    if (len == SUBMODULE_CAPTURE_MIN_LEN || len == SUBMODULE_CAPTURE_WITH_ID_LEN) {
      result.captureSucceeded = applyCaptureImage(state, ops, frame, len);
      result.sleepAfterCommand = true;
      return result;
    }
    Serial.printf("CAPTURE: ignored — expected 1 or 3 bytes, got %u\n",
                  (unsigned)len);
    return result;
  }

  Serial.printf("Unknown command 0x%02X (%u bytes) — ignored\n", cmd, (unsigned)len);
  return result;
}
