#pragma once

#include <stdint.h>

struct SenseDateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
};

bool senseSdCardReady();
const char *senseCaptureLastError();

void senseSetCaptureDebug(bool enabled);
bool senseCaptureDebugEnabled();

bool senseCaptureImageDatetime(const SenseDateTime *dt);
bool senseCaptureImageById(uint16_t imageId);
