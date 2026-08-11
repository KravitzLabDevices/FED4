#include "SubmoduleMaster.h"

#include <Arduino.h>
#include <Wire.h>

const char *submoduleMasterTxErrorStr(uint8_t err) {
  switch (err) {
    case 0:
      return "OK";
    case 1:
      return "data too long";
    case 2:
      return "NAK on address";
    case 3:
      return "NAK on data";
    case 4:
      return "other error";
    case 5:
      return "timeout";
    default:
      return "unknown";
  }
}

uint8_t submoduleMasterProbe(const SubmoduleMasterConfig *config) {
  if (config == nullptr) {
    return 4;
  }
  Wire.beginTransmission(config->addr);
  return Wire.endTransmission(true);
}

void submoduleMasterRecoverBus(const SubmoduleMasterConfig *config) {
  if (config == nullptr) {
    return;
  }

  Wire.end();
  pinMode(config->sclPin, INPUT_PULLUP);
  pinMode(config->sdaPin, INPUT_PULLUP);
  delay(1);

  pinMode(config->sclPin, OUTPUT);
  digitalWrite(config->sclPin, HIGH);
  for (int i = 0; i < 9 && digitalRead(config->sdaPin) == LOW; i++) {
    digitalWrite(config->sclPin, LOW);
    delayMicroseconds(5);
    digitalWrite(config->sclPin, HIGH);
    delayMicroseconds(5);
  }

  pinMode(config->sclPin, INPUT_PULLUP);
  pinMode(config->sdaPin, INPUT_PULLUP);
  Wire.begin(config->sdaPin, config->sclPin, config->freqHz);
  Wire.setTimeout(1000);
}

bool submoduleMasterWakeAndWait(const SubmoduleMasterConfig *config) {
  if (config == nullptr) {
    return false;
  }

  const uint8_t wakeErr = submoduleMasterProbe(config);
  if (wakeErr == 4) {
    submoduleMasterRecoverBus(config);
  }

  const unsigned long deadline = millis() + config->readyTimeoutMs;
  while (millis() < deadline) {
    delay(config->readyPollMs);
    const uint8_t err = submoduleMasterProbe(config);
    if (err == 0) {
      return true;
    }
    if (err == 4) {
      submoduleMasterRecoverBus(config);
    }
  }

  return false;
}

bool submoduleMasterReadStatus(const SubmoduleMasterConfig *config,
                               SubmoduleStatus *status) {
  if (config == nullptr || status == nullptr) {
    return false;
  }

  const uint8_t received =
      Wire.requestFrom(config->addr, (uint8_t)SUBMODULE_STATUS_READ_LEN);
  if (received != SUBMODULE_STATUS_READ_LEN) {
    return false;
  }

  status->flags = (uint8_t)Wire.read();
  status->lastError = (uint8_t)Wire.read();
  return true;
}

bool submoduleMasterSend(const SubmoduleMasterConfig *config,
                         const uint8_t *data, size_t len) {
  if (config == nullptr || (data == nullptr && len > 0)) {
    return false;
  }

  for (int attempt = 1; attempt <= 2; attempt++) {
    Wire.beginTransmission(config->addr);
    for (size_t i = 0; i < len; i++) {
      Wire.write(data[i]);
    }
    const uint8_t err = Wire.endTransmission(true);
    if (err == 0) {
      return true;
    }
    if (err == 4) {
      submoduleMasterRecoverBus(config);
    }
    if (attempt == 1) {
      delay(config->txRetryDelayMs);
    }
  }
  return false;
}

bool submoduleMasterWakeReadStatus(const SubmoduleMasterConfig *config,
                                   SubmoduleStatus *status) {
  if (!submoduleMasterWakeAndWait(config)) {
    return false;
  }
  return submoduleMasterReadStatus(config, status);
}
