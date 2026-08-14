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

bool submoduleApplyDateTime(SubmoduleDateTime *outRtc, bool *outRtcValid,
                            uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t min, uint8_t sec);

// Parse "T yyyy mm dd HH MM SS" (leading T optional if already stripped).
bool submoduleApplySetTimeLine(SubmoduleDateTime *outRtc, bool *outRtcValid,
                               const char *line);

bool submoduleGetCurrentDateTime(SubmoduleDateTime *out);

void submoduleFormatDatetimeFilename(char *out, size_t outLen,
                                     const SubmoduleDateTime *dt);
