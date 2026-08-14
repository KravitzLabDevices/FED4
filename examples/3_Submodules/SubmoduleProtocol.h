/*
 * FED4 Submodule TRRS UART Protocol — shared text frames.
 * Spec: examples/3_Submodules/README.md
 *
 * Init / UART session only (capture is TRIG fire-and-forget, no OK handshake).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint32_t SUBMODULE_UART_BAUD = 115200;

static const uint8_t SUBMODULE_ERR_NONE = 0;
static const uint8_t SUBMODULE_ERR_BAD_CMD = 1;
static const uint8_t SUBMODULE_ERR_RTC_NOT_SET = 2;
static const uint8_t SUBMODULE_ERR_CAPTURE = 3;
static const uint8_t SUBMODULE_ERR_RENAME = 4;
static const uint8_t SUBMODULE_ERR_UNKNOWN = 255;

static inline void submoduleFormatSetTime(char *out, size_t outLen, uint16_t year,
                                          uint8_t month, uint8_t day, uint8_t hour,
                                          uint8_t min, uint8_t sec) {
  snprintf(out, outLen, "T %u %u %u %u %u %u\n", (unsigned)year, (unsigned)month,
           (unsigned)day, (unsigned)hour, (unsigned)min, (unsigned)sec);
}

static inline void submoduleFormatRename(char *out, size_t outLen, const char *name) {
  snprintf(out, outLen, "R %s\n", name != nullptr ? name : "");
}

static inline bool submoduleLineIsRdy(const char *line) {
  return line != nullptr && strncmp(line, "RDY", 3) == 0;
}

static inline bool submoduleLineIsOk(const char *line) {
  return line != nullptr && strncmp(line, "OK", 2) == 0;
}

static inline bool submoduleLineIsErr(const char *line, uint8_t *outCode) {
  if (line == nullptr || strncmp(line, "ERR", 3) != 0) {
    return false;
  }
  if (outCode != nullptr) {
    unsigned code = SUBMODULE_ERR_UNKNOWN;
    sscanf(line + 3, " %u", &code);
    *outCode = (uint8_t)code;
  }
  return true;
}

static inline const char *submoduleErrName(uint8_t code) {
  switch (code) {
    case SUBMODULE_ERR_NONE:
      return "none";
    case SUBMODULE_ERR_BAD_CMD:
      return "bad command";
    case SUBMODULE_ERR_RTC_NOT_SET:
      return "RTC not set";
    case SUBMODULE_ERR_CAPTURE:
      return "capture failed";
    case SUBMODULE_ERR_RENAME:
      return "rename failed";
    default:
      return "unknown";
  }
}
