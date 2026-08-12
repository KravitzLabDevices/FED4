/*
 * SeeedStudio Sense — I2C Slave Submodule (v0.3)
 *
 * Board-specific wiring and capture backend. Shared protocol, state, commands,
 * and ESP32-S3 I2C slave transport live in examples/3_Submodules/.
 * Spec: ../README.md
 *
 * Pins (XIAO ESP32-S3 Sense):
 *   D4 / SDA = GPIO5
 *   D5 / SCL = GPIO6
 *   SD SPI: CS=GPIO21, SCK=GPIO7, MISO=GPIO8, MOSI=GPIO9 (expansion board)
 *
 * Arduino IDE:
 *   Board: Seeed Studio XIAO ESP32S3 Sense
 *   USB CDC On Boot: Enabled
 */

#include <Arduino.h>
#include "../SubmoduleCommands.h"
#include "../SubmoduleI2cSlaveEsp32.h"
#include "../SubmoduleState.h"
#include "SenseCamera.h"

static const uint32_t SERIAL_BOOT_DELAY_MS = 2000;

static SubmoduleState gState;
static SubmoduleI2cSlaveEsp32 gI2cSlave;

static bool captureDatetimeAdapter(const SubmoduleDateTime *dt) {
  return senseCaptureImageDatetime(dt);
}

static bool captureByIdAdapter(uint16_t imageId) {
  return senseCaptureImageById(imageId);
}

static void releaseBusAdapter(void) {
  submoduleI2cSlaveEsp32End(&gI2cSlave);
}

static const SubmoduleCaptureOps kCaptureOps = {
    captureDatetimeAdapter,
    captureByIdAdapter,
    senseCaptureLastError,
    releaseBusAdapter,
};

static bool onCommand(const uint8_t *frame, size_t len,
                      SubmoduleCommandResult *result, void *context) {
  (void)context;
  *result = submoduleDispatchCommand(&gState, &kCaptureOps, frame, len);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  submoduleStateInit(&gState);

  const SubmoduleI2cSlaveEsp32Config i2cConfig = {
      SUBMODULE_I2C_ADDR,
      SDA,
      SCL,
      100000,
      9000,
      20,
      50000,
      15,
      16,
  };

  if (!submoduleI2cSlaveEsp32Init(&gI2cSlave, &i2cConfig, &gState)) {
    Serial.println("I2C slave init FAILED");
    while (true) {
      delay(1000);
    }
  }

  Serial.println();
  Serial.println("SeeedStudio Sense I2C slave submodule v0.3");
  Serial.printf("Address: 0x%02X  SDA: D4/GPIO%d  SCL: D5/GPIO%d  %lu kHz\n",
                SUBMODULE_I2C_ADDR, SDA, SCL, (unsigned long)(i2cConfig.freqHz / 1000));
  Serial.println("Shared modules: SubmoduleProtocol, State, Rtc, Commands, I2cSlaveEsp32");
  Serial.println("Board backend: SenseCamera (OV3660 + microSD)");

  gState.sdReady = senseSdCardReady();
  Serial.println(gState.sdReady ? "SD card: detected at boot"
                                : "SD card: NOT detected at boot — check card + J3 pad");

  if (!submoduleI2cSlaveEsp32Begin(&gI2cSlave)) {
    Serial.println("I2C slave begin FAILED at boot");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Entering sleep loop");
}

void loop() {
  submoduleI2cSlaveEsp32EnterLightSleep(&gI2cSlave);
  submoduleI2cSlaveEsp32HandleWake(&gI2cSlave, onCommand, nullptr);
}
