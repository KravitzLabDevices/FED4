#pragma once

#include <stddef.h>
#include <stdint.h>
#include "SubmoduleRtc.h"
#include "SubmoduleState.h"

typedef struct {
  bool (*captureDatetime)(const SubmoduleDateTime *rtcTime);
  bool (*captureById)(uint16_t imageId);
  const char *(*captureLastError)(void);
  void (*releaseBus)(void);
} SubmoduleCaptureOps;

typedef struct {
  bool sleepAfterCommand;
  bool captureSucceeded;
} SubmoduleCommandResult;

SubmoduleCommandResult submoduleDispatchCommand(SubmoduleState *state,
                                                  const SubmoduleCaptureOps *ops,
                                                  const uint8_t *frame,
                                                  size_t len);
