#include "FED4.h"

// SeeedStudio Sense (and future) I2C submodule — master side on FED4.
// Spec: examples/3_Submodules/README.md
// Gated by FED4_ENABLE_SUBMODULE in FED4.h (default 0).

#if FED4_ENABLE_SUBMODULE

namespace {
constexpr uint32_t kSenseReadyPollMs = 50;
constexpr uint32_t kSenseReadyTimeoutMs = 2000;
constexpr uint32_t kSenseTxRetryDelayMs = 100;
uint8_t senseProbeAddr()
{
  Wire.beginTransmission(SUBMODULE_I2C_ADDR);
  return Wire.endTransmission(true);
}
} // namespace

bool FED4::senseWakeAndWait()
{
  const uint8_t wakeErr = senseProbeAddr();
  if (wakeErr == 4)
  {
    i2cRecoverBus();
  }

  const unsigned long deadline = millis() + kSenseReadyTimeoutMs;
  while (millis() < deadline)
  {
    delay(kSenseReadyPollMs);
    const uint8_t err = senseProbeAddr();
    if (err == 0)
    {
      return true;
    }
    if (err == 4)
    {
      i2cRecoverBus();
    }
  }
  return false;
}

bool FED4::sensePresent()
{
  return senseWakeAndWait();
}

bool FED4::senseReadStatus(SubmoduleStatus *status)
{
  if (status == nullptr)
  {
    return false;
  }

  const uint8_t received =
      Wire.requestFrom((uint8_t)SUBMODULE_I2C_ADDR, (uint8_t)SUBMODULE_STATUS_READ_LEN);
  if (received != SUBMODULE_STATUS_READ_LEN)
  {
    return false;
  }

  status->flags = (uint8_t)Wire.read();
  status->lastError = (uint8_t)Wire.read();
  return true;
}

bool FED4::senseWakeReadStatus(SubmoduleStatus *status)
{
  if (!senseWakeAndWait())
  {
    return false;
  }
  return senseReadStatus(status);
}

bool FED4::senseSend(const uint8_t *data, size_t len)
{
  if (data == nullptr && len > 0)
  {
    return false;
  }

  for (int attempt = 1; attempt <= 2; attempt++)
  {
    Wire.beginTransmission(SUBMODULE_I2C_ADDR);
    for (size_t i = 0; i < len; i++)
    {
      Wire.write(data[i]);
    }
    const uint8_t err = Wire.endTransmission(true);
    if (err == 0)
    {
      return true;
    }
    if (err == 4)
    {
      i2cRecoverBus();
    }
    if (attempt == 1)
    {
      delay(kSenseTxRetryDelayMs);
    }
  }
  return false;
}

bool FED4::senseSyncTime()
{
  SubmoduleStatus status = {};
  if (!senseWakeReadStatus(&status))
  {
    Serial.println("Sense: not ready — SET_TIME skipped");
    return false;
  }

  DateTime t = now();
  uint8_t frame[SUBMODULE_SET_TIME_FRAME_LEN];
  const size_t len = submodulePackSetTime(
      frame, (uint16_t)t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());

  if (!senseSend(frame, len))
  {
    Serial.println("Sense: SET_TIME failed");
    return false;
  }
  return true;
}

bool FED4::senseCapture(uint32_t settleMs)
{
  SubmoduleStatus status = {};
  if (!senseWakeReadStatus(&status))
  {
    Serial.println("Sense: not ready — CAPTURE skipped");
    return false;
  }

  uint8_t frame[SUBMODULE_CAPTURE_WITH_ID_LEN];
  const size_t len = submodulePackCaptureDatetime(frame);
  if (!senseSend(frame, len))
  {
    Serial.println("Sense: CAPTURE failed");
    return false;
  }

  if (settleMs > 0)
  {
    delay(settleMs);
  }
  return true;
}

bool FED4::senseSyncAndCapture(uint32_t settleMs)
{
  if (!senseSyncTime())
  {
    return false;
  }
  // CAPTURE follows immediately; senseCapture()'s wake/status path already
  // gives the slave listen loop time to apply SET_TIME before RTC-named shots.
  return senseCapture(settleMs);
}

#endif // FED4_ENABLE_SUBMODULE
