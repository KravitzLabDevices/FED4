#pragma once

#include <stddef.h>
#include <stdint.h>
#include "SubmoduleCommands.h"
#include "SubmoduleState.h"

typedef struct {
  uint8_t addr;
  int sdaPin;
  int sclPin;
  uint32_t freqHz;
  uint32_t listenMs;
  uint32_t postWakeSettleMs;
  uint32_t busIdleTimeoutUs;
  int initRetries;
  size_t rxBufSize;
} SubmoduleI2cSlaveEsp32Config;

typedef bool (*SubmoduleListenCallback)(const uint8_t *frame, size_t len,
                                        SubmoduleCommandResult *result,
                                        void *context);

typedef struct {
  SubmoduleI2cSlaveEsp32Config config;
  SubmoduleState *state;
  bool active;
  uint8_t rxBuf[16];
  volatile size_t rxLen;
  volatile bool rxPending;
} SubmoduleI2cSlaveEsp32;

bool submoduleI2cSlaveEsp32Init(SubmoduleI2cSlaveEsp32 *slave,
                                const SubmoduleI2cSlaveEsp32Config *config,
                                SubmoduleState *state);

void submoduleI2cSlaveEsp32End(SubmoduleI2cSlaveEsp32 *slave);

bool submoduleI2cSlaveEsp32Begin(SubmoduleI2cSlaveEsp32 *slave);

void submoduleI2cSlaveEsp32EnterLightSleep(SubmoduleI2cSlaveEsp32 *slave);

bool submoduleI2cSlaveEsp32HandleWake(SubmoduleI2cSlaveEsp32 *slave,
                                      SubmoduleListenCallback callback,
                                      void *context);

const char *submoduleI2cSlaveEsp32WakeupCauseStr(void);
