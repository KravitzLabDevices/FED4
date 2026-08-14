#pragma once

#include <stddef.h>
#include <stdint.h>
#include "SubmoduleState.h"

typedef struct {
  int trigPin;   // idle HIGH, active LOW
  int dataPin;   // half-duplex UART RX+TX
  uint32_t baud;
} SubmoduleUartEsp32Config;

typedef struct {
  SubmoduleUartEsp32Config config;
  SubmoduleState *state;
  bool uartUp;
  bool capturing;
} SubmoduleUartEsp32;

typedef bool (*SubmoduleCaptureFn)(SubmoduleState *state);

bool submoduleUartEsp32Init(SubmoduleUartEsp32 *ctx,
                            const SubmoduleUartEsp32Config *config,
                            SubmoduleState *state);

void submoduleUartEsp32EndUart(SubmoduleUartEsp32 *ctx);
bool submoduleUartEsp32BeginUart(SubmoduleUartEsp32 *ctx);
void submoduleUartEsp32SendRdy(SubmoduleUartEsp32 *ctx);

// Light-sleep until TRIG goes LOW (level wake).
void submoduleUartEsp32EnterLightSleep(SubmoduleUartEsp32 *ctx);

// After wake: if rtcValid, run capture (blocks sleep), then poll TRIG/UART
// until TRIG HIGH, then return (caller should sleep again).
void submoduleUartEsp32HandleWakeSession(SubmoduleUartEsp32 *ctx,
                                         SubmoduleCaptureFn captureFn);
