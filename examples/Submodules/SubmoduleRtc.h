#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
} SubmoduleDateTime;

bool submoduleValidateDateTime(uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t min, uint8_t sec);

bool submoduleApplySetTimeFrame(SubmoduleDateTime *outRtc, bool *outRtcValid,
                                const uint8_t *frame, size_t len);

// Read live wall clock previously set via SET_TIME / settimeofday().
bool submoduleGetCurrentDateTime(SubmoduleDateTime *out);
