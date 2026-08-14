#pragma once

#include "SubmoduleRtc.h"

typedef struct {
  bool rtcValid;
  SubmoduleDateTime rtcTime;
  char lastFilename[32];
  uint8_t lastErrorCode;
} SubmoduleState;

void submoduleStateInit(SubmoduleState *state);
void submoduleStateSetError(SubmoduleState *state, uint8_t code);
