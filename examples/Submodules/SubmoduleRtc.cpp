#include "SubmoduleRtc.h"

#include <sys/time.h>
#include <time.h>
#include "SubmoduleProtocol.h"

bool submoduleValidateDateTime(uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t min, uint8_t sec) {
  if (year < 2000 || year > 2099) {
    return false;
  }
  if (month < 1 || month > 12) {
    return false;
  }
  if (day < 1 || day > 31) {
    return false;
  }
  if (hour > 23 || min > 59 || sec > 59) {
    return false;
  }
  return true;
}

bool submoduleApplySetTimeFrame(SubmoduleDateTime *outRtc, bool *outRtcValid,
                                const uint8_t *frame, size_t len) {
  if (outRtc == nullptr || outRtcValid == nullptr || frame == nullptr) {
    return false;
  }
  if (len != SUBMODULE_SET_TIME_FRAME_LEN || frame[0] != SUBMODULE_CMD_SET_TIME) {
    return false;
  }

  const uint16_t year = submoduleReadU16Le(&frame[1]);
  const uint8_t month = frame[3];
  const uint8_t day = frame[4];
  const uint8_t hour = frame[5];
  const uint8_t min = frame[6];
  const uint8_t sec = frame[7];

  if (!submoduleValidateDateTime(year, month, day, hour, min, sec)) {
    return false;
  }

  struct tm tmTime = {};
  tmTime.tm_year = (int)year - 1900;
  tmTime.tm_mon = (int)month - 1;
  tmTime.tm_mday = (int)day;
  tmTime.tm_hour = (int)hour;
  tmTime.tm_min = (int)min;
  tmTime.tm_sec = (int)sec;
  tmTime.tm_isdst = -1;

  struct timeval tv = {};
  tv.tv_sec = mktime(&tmTime);
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) {
    return false;
  }

  outRtc->year = year;
  outRtc->month = month;
  outRtc->day = day;
  outRtc->hour = hour;
  outRtc->min = min;
  outRtc->sec = sec;
  *outRtcValid = true;
  return true;
}

bool submoduleGetCurrentDateTime(SubmoduleDateTime *out) {
  if (out == nullptr) {
    return false;
  }

  struct timeval tv = {};
  if (gettimeofday(&tv, nullptr) != 0) {
    return false;
  }

  struct tm tmTime = {};
  if (localtime_r(&tv.tv_sec, &tmTime) == nullptr) {
    return false;
  }

  const uint16_t year = (uint16_t)(tmTime.tm_year + 1900);
  const uint8_t month = (uint8_t)(tmTime.tm_mon + 1);
  const uint8_t day = (uint8_t)tmTime.tm_mday;
  const uint8_t hour = (uint8_t)tmTime.tm_hour;
  const uint8_t min = (uint8_t)tmTime.tm_min;
  const uint8_t sec = (uint8_t)tmTime.tm_sec;

  if (!submoduleValidateDateTime(year, month, day, hour, min, sec)) {
    return false;
  }

  out->year = year;
  out->month = month;
  out->day = day;
  out->hour = hour;
  out->min = min;
  out->sec = sec;
  return true;
}
