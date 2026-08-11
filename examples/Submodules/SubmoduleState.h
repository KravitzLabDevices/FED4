#pragma once

#include <stdint.h>
#include "SubmoduleProtocol.h"
#include "SubmoduleRtc.h"

typedef struct {
  bool rtcValid;
  SubmoduleDateTime rtcTime;
  bool sdReady;
  uint8_t lastErrorCode;
} SubmoduleState;

void submoduleStateInit(SubmoduleState *state);
uint8_t submoduleStateBuildFlags(const SubmoduleState *state);
void submoduleStateSetError(SubmoduleState *state, uint8_t code);
void submoduleStateMapCaptureError(SubmoduleState *state, const char *message);
