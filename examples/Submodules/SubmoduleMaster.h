#pragma once

#include <stddef.h>
#include <stdint.h>
#include "SubmoduleProtocol.h"

typedef struct {
  uint8_t addr;
  int sdaPin;
  int sclPin;
  uint32_t freqHz;
  uint32_t readyPollMs;
  uint32_t readyTimeoutMs;
  uint32_t txRetryDelayMs;
} SubmoduleMasterConfig;

const char *submoduleMasterTxErrorStr(uint8_t err);

uint8_t submoduleMasterProbe(const SubmoduleMasterConfig *config);

void submoduleMasterRecoverBus(const SubmoduleMasterConfig *config);

bool submoduleMasterWakeAndWait(const SubmoduleMasterConfig *config);

bool submoduleMasterReadStatus(const SubmoduleMasterConfig *config,
                               SubmoduleStatus *status);

bool submoduleMasterSend(const SubmoduleMasterConfig *config,
                         const uint8_t *data, size_t len);

bool submoduleMasterWakeReadStatus(const SubmoduleMasterConfig *config,
                                   SubmoduleStatus *status);
