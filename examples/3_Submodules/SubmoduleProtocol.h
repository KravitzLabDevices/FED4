/*
 * FED4 Submodule I2C Protocol — shared constants and pack helpers.
 * Spec: examples/3_Submodules/README.md
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

static const uint8_t SUBMODULE_I2C_ADDR = 0x42;

static const uint8_t SUBMODULE_PROTOCOL_VERSION = 0x03;
static const size_t SUBMODULE_STATUS_READ_LEN = 2;

static const uint8_t SUBMODULE_CMD_RESERVED = 0x00;
static const uint8_t SUBMODULE_CMD_SET_TIME = 0x01;
static const uint8_t SUBMODULE_CMD_CAPTURE_IMAGE = 0x02;

static const size_t SUBMODULE_SET_TIME_FRAME_LEN = 8;
static const size_t SUBMODULE_CAPTURE_MIN_LEN = 1;
static const size_t SUBMODULE_CAPTURE_WITH_ID_LEN = 3;

// Status byte 0 flags (bits 0–3); bits 4–7 = protocol version nibble
static const uint8_t SUBMODULE_STATUS_RTC_VALID = 0x01;
static const uint8_t SUBMODULE_STATUS_SD_READY = 0x02;
static const uint8_t SUBMODULE_STATUS_BUSY = 0x04;
static const uint8_t SUBMODULE_STATUS_PROTO_SHIFT = 4;

// Status byte 1 — last error from most recent capture attempt
static const uint8_t SUBMODULE_ERR_NONE = 0;
static const uint8_t SUBMODULE_ERR_SD_MOUNT = 1;
static const uint8_t SUBMODULE_ERR_SD_NOT_DETECTED = 2;
static const uint8_t SUBMODULE_ERR_SD_WRITE = 3;
static const uint8_t SUBMODULE_ERR_CAMERA_INIT = 4;
static const uint8_t SUBMODULE_ERR_CAPTURE_FRAME = 5;
static const uint8_t SUBMODULE_ERR_RTC_NOT_SET = 6;
static const uint8_t SUBMODULE_ERR_UNKNOWN = 255;

typedef struct {
  uint8_t flags;
  uint8_t lastError;
} SubmoduleStatus;

static inline uint8_t submoduleBuildStatusByte(uint8_t flags) {
  return (uint8_t)((SUBMODULE_PROTOCOL_VERSION << SUBMODULE_STATUS_PROTO_SHIFT) |
                   (flags & 0x0F));
}

static inline uint8_t submoduleStatusProtocolVersion(uint8_t statusByte) {
  return (uint8_t)(statusByte >> SUBMODULE_STATUS_PROTO_SHIFT);
}

static inline bool submoduleStatusRtcValid(uint8_t flags) {
  return (flags & SUBMODULE_STATUS_RTC_VALID) != 0;
}

static inline bool submoduleStatusSdReady(uint8_t flags) {
  return (flags & SUBMODULE_STATUS_SD_READY) != 0;
}

static inline bool submoduleStatusBusy(uint8_t flags) {
  return (flags & SUBMODULE_STATUS_BUSY) != 0;
}

static inline uint16_t submoduleReadU16Le(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline void submoduleWriteU16Le(uint8_t *data, uint16_t value) {
  data[0] = (uint8_t)(value & 0xFF);
  data[1] = (uint8_t)((value >> 8) & 0xFF);
}

static inline size_t submodulePackSetTime(uint8_t *out, uint16_t year,
                                          uint8_t month, uint8_t day,
                                          uint8_t hour, uint8_t min,
                                          uint8_t sec) {
  out[0] = SUBMODULE_CMD_SET_TIME;
  submoduleWriteU16Le(&out[1], year);
  out[3] = month;
  out[4] = day;
  out[5] = hour;
  out[6] = min;
  out[7] = sec;
  return SUBMODULE_SET_TIME_FRAME_LEN;
}

static inline size_t submodulePackCaptureDatetime(uint8_t *out) {
  out[0] = SUBMODULE_CMD_CAPTURE_IMAGE;
  return SUBMODULE_CAPTURE_MIN_LEN;
}

static inline size_t submodulePackCaptureWithId(uint8_t *out, uint16_t imageId) {
  out[0] = SUBMODULE_CMD_CAPTURE_IMAGE;
  submoduleWriteU16Le(&out[1], imageId);
  return SUBMODULE_CAPTURE_WITH_ID_LEN;
}

static inline const char *submoduleErrorStr(uint8_t code) {
  switch (code) {
    case SUBMODULE_ERR_NONE:
      return "none";
    case SUBMODULE_ERR_SD_MOUNT:
      return "SD mount failed";
    case SUBMODULE_ERR_SD_NOT_DETECTED:
      return "SD card not detected";
    case SUBMODULE_ERR_SD_WRITE:
      return "SD write failed";
    case SUBMODULE_ERR_CAMERA_INIT:
      return "camera init failed";
    case SUBMODULE_ERR_CAPTURE_FRAME:
      return "frame capture failed";
    case SUBMODULE_ERR_RTC_NOT_SET:
      return "RTC not set";
    default:
      return "unknown";
  }
}
