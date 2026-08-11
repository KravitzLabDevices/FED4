#include "SubmoduleI2cSlaveEsp32.h"

#include <Arduino.h>
#include <Wire.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

static SubmoduleI2cSlaveEsp32 *gActiveSlave = nullptr;

static void onRequest() {
  if (gActiveSlave == nullptr || gActiveSlave->state == nullptr) {
    Wire.write((uint8_t)0);
    Wire.write((uint8_t)0);
    return;
  }
  Wire.write(submoduleStateBuildFlags(gActiveSlave->state));
  Wire.write(gActiveSlave->state->lastErrorCode);
}

static void onReceive(int byteCount) {
  if (gActiveSlave == nullptr) {
    return;
  }

  const size_t maxLen = gActiveSlave->config.rxBufSize > 0
                            ? gActiveSlave->config.rxBufSize
                            : sizeof(gActiveSlave->rxBuf);
  gActiveSlave->rxLen = 0;
  while (Wire.available() && gActiveSlave->rxLen < maxLen) {
    gActiveSlave->rxBuf[gActiveSlave->rxLen++] = (uint8_t)Wire.read();
  }
  while (Wire.available()) {
    Wire.read();
  }
  gActiveSlave->rxPending = true;
  (void)byteCount;
}

static void clearRx(SubmoduleI2cSlaveEsp32 *slave) {
  if (slave == nullptr) {
    return;
  }
  slave->rxLen = 0;
  slave->rxPending = false;
}

static void configureSleepBusPins(int sdaPin, int sclPin) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << sdaPin) | (1ULL << sclPin);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
}

static bool prepareBusAfterWake(const SubmoduleI2cSlaveEsp32 *slave) {
  const int sdaPin = slave->config.sdaPin;
  const int sclPin = slave->config.sclPin;

  gpio_reset_pin((gpio_num_t)sclPin);
  gpio_reset_pin((gpio_num_t)sdaPin);

  pinMode(sdaPin, INPUT);
  pinMode(sclPin, INPUT);

  const unsigned long start = micros();
  while ((digitalRead(sclPin) == LOW || digitalRead(sdaPin) == LOW) &&
         (unsigned long)(micros() - start) < slave->config.busIdleTimeoutUs) {
    delayMicroseconds(100);
  }

  if (digitalRead(sclPin) == HIGH && digitalRead(sdaPin) == HIGH) {
    return true;
  }

  pinMode(sdaPin, INPUT);
  pinMode(sclPin, OUTPUT_OPEN_DRAIN);
  digitalWrite(sclPin, HIGH);

  for (int i = 0; i < 9 && digitalRead(sdaPin) == LOW; i++) {
    digitalWrite(sclPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
  }

  pinMode(sclPin, INPUT);
  pinMode(sdaPin, INPUT);
  return digitalRead(sclPin) == HIGH && digitalRead(sdaPin) == HIGH;
}

bool submoduleI2cSlaveEsp32Init(SubmoduleI2cSlaveEsp32 *slave,
                                const SubmoduleI2cSlaveEsp32Config *config,
                                SubmoduleState *state) {
  if (slave == nullptr || config == nullptr || state == nullptr) {
    return false;
  }

  slave->config = *config;
  if (slave->config.rxBufSize == 0 || slave->config.rxBufSize > sizeof(slave->rxBuf)) {
    slave->config.rxBufSize = sizeof(slave->rxBuf);
  }
  slave->state = state;
  slave->active = false;
  slave->rxLen = 0;
  slave->rxPending = false;
  gActiveSlave = slave;
  return true;
}

void submoduleI2cSlaveEsp32End(SubmoduleI2cSlaveEsp32 *slave) {
  if (slave != nullptr && slave->active) {
    Wire.end();
    slave->active = false;
  }
  if (gActiveSlave == slave) {
    gActiveSlave = nullptr;
  }
}

bool submoduleI2cSlaveEsp32Begin(SubmoduleI2cSlaveEsp32 *slave) {
  if (slave == nullptr) {
    return false;
  }

  submoduleI2cSlaveEsp32End(slave);

  gpio_reset_pin((gpio_num_t)slave->config.sclPin);
  gpio_reset_pin((gpio_num_t)slave->config.sdaPin);

  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  slave->active = Wire.begin(slave->config.addr, slave->config.sdaPin,
                             slave->config.sclPin, slave->config.freqHz);
  gActiveSlave = slave;
  return slave->active;
}

void submoduleI2cSlaveEsp32EnterLightSleep(SubmoduleI2cSlaveEsp32 *slave) {
  if (slave == nullptr) {
    return;
  }

  submoduleI2cSlaveEsp32End(slave);
  configureSleepBusPins(slave->config.sdaPin, slave->config.sclPin);

  gpio_wakeup_enable((gpio_num_t)slave->config.sclPin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  Serial.println("Sleeping (SCL wake)...");
  Serial.flush();
  esp_light_sleep_start();
}

const char *submoduleI2cSlaveEsp32WakeupCauseStr(void) {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_GPIO:
      return "GPIO (SCL)";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return "undefined (boot)";
    default:
      return "other";
  }
}

bool submoduleI2cSlaveEsp32HandleWake(SubmoduleI2cSlaveEsp32 *slave,
                                      SubmoduleListenCallback callback,
                                      void *context) {
  if (slave == nullptr || callback == nullptr) {
    return false;
  }

  const unsigned long wakeMs = millis();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  delay(slave->config.postWakeSettleMs);
  prepareBusAfterWake(slave);

  bool ready = false;
  for (int attempt = 1; attempt <= slave->config.initRetries; attempt++) {
    if (submoduleI2cSlaveEsp32Begin(slave)) {
      ready = true;
      break;
    }
    delay(20);
  }

  if (!ready) {
    Serial.printf("Wake: %s — I2C slave init FAILED — SDA=%d SCL=%d\n",
                  submoduleI2cSlaveEsp32WakeupCauseStr(),
                  digitalRead(slave->config.sdaPin),
                  digitalRead(slave->config.sclPin));
    return false;
  }

  Serial.printf("Wake: %s — I2C slave ready @ 0x%02X (+%lu ms, rtcValid=%s)\n",
                submoduleI2cSlaveEsp32WakeupCauseStr(), slave->config.addr,
                millis() - wakeMs, slave->state->rtcValid ? "true" : "false");

  const unsigned long listenUntil = millis() + slave->config.listenMs;
  SubmoduleCommandResult lastResult = {};

  while (millis() < listenUntil && !lastResult.sleepAfterCommand) {
    if (slave->rxPending) {
      lastResult = {};
      (void)callback(slave->rxBuf, slave->rxLen, &lastResult, context);
      clearRx(slave);
    }
    delay(5);
  }

  if (lastResult.sleepAfterCommand) {
    if (lastResult.captureSucceeded) {
      Serial.println("CAPTURE complete — returning to sleep");
    } else {
      Serial.println("CAPTURE failed — returning to sleep");
    }
    Serial.flush();
    delay(100);
  }

  return true;
}
