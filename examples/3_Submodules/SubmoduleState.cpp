#include "SubmoduleState.h"

#include <string.h>

void submoduleStateInit(SubmoduleState *state) {
  if (state == nullptr) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->lastErrorCode = SUBMODULE_ERR_NONE;
}

uint8_t submoduleStateBuildFlags(const SubmoduleState *state) {
  if (state == nullptr) {
    return submoduleBuildStatusByte(0);
  }

  uint8_t flags = 0;
  if (state->rtcValid) {
    flags |= SUBMODULE_STATUS_RTC_VALID;
  }
  if (state->sdReady) {
    flags |= SUBMODULE_STATUS_SD_READY;
  }
  return submoduleBuildStatusByte(flags);
}

void submoduleStateSetError(SubmoduleState *state, uint8_t code) {
  if (state != nullptr) {
    state->lastErrorCode = code;
  }
}

void submoduleStateMapCaptureError(SubmoduleState *state, const char *message) {
  if (state == nullptr) {
    return;
  }
  if (message == nullptr || message[0] == '\0') {
    submoduleStateSetError(state, SUBMODULE_ERR_UNKNOWN);
    return;
  }
  if (strstr(message, "SD mount failed") != nullptr) {
    submoduleStateSetError(state, SUBMODULE_ERR_SD_MOUNT);
  } else if (strstr(message, "SD card not detected") != nullptr ||
             strstr(message, "SD card unreadable") != nullptr) {
    submoduleStateSetError(state, SUBMODULE_ERR_SD_NOT_DETECTED);
  } else if (strstr(message, "SD write") != nullptr ||
             strstr(message, "SD file open failed") != nullptr) {
    submoduleStateSetError(state, SUBMODULE_ERR_SD_WRITE);
  } else if (strstr(message, "camera init failed") != nullptr) {
    submoduleStateSetError(state, SUBMODULE_ERR_CAMERA_INIT);
  } else if (strstr(message, "camera frame capture failed") != nullptr) {
    submoduleStateSetError(state, SUBMODULE_ERR_CAPTURE_FRAME);
  } else {
    submoduleStateSetError(state, SUBMODULE_ERR_UNKNOWN);
  }
}
