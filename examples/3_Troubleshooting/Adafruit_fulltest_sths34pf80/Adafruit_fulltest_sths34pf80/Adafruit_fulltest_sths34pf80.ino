/*
 * STHS34PF80 Full Test Sketch on FED4
 * 
 * This sketch demonstrates comprehensive testing of all STHS34PF80 sensor settings
 * and configuration options. It configures each parameter using the sensor's default
 * values to demonstrate the API without changing register states unexpectedly.
 * 
 * The sketch will halt with an error message if any configuration fails, and will
 * continuously display real-time sensor readings including temperature, motion,
 * presence detection, and temperature shock detection.
 * 
 * Note: All setter functions use the actual sensor default values discovered through
 * testing to provide a stable demonstration of the configuration API.
 */

#include "Adafruit_STHS34PF80.h"
#include <Wire.h>

// Define I2C_2 bus with FED4 pins
#define SDA_2 20
#define SCL_2 19
TwoWire I2C_2 = TwoWire(0);  // Use I2C peripheral 0

Adafruit_STHS34PF80 sths;

void printLPFSetting(sths34pf80_lpf_config_t lpf_setting) {
  switch (lpf_setting) {
    case STHS34PF80_LPF_ODR_DIV_9:
      Serial.print("ODR/9");
      break;
    case STHS34PF80_LPF_ODR_DIV_20:
      Serial.print("ODR/20");
      break;
    case STHS34PF80_LPF_ODR_DIV_50:
      Serial.print("ODR/50");
      break;
    case STHS34PF80_LPF_ODR_DIV_100:
      Serial.print("ODR/100");
      break;
    case STHS34PF80_LPF_ODR_DIV_200:
      Serial.print("ODR/200");
      break;
    case STHS34PF80_LPF_ODR_DIV_400:
      Serial.print("ODR/400");
      break;
    case STHS34PF80_LPF_ODR_DIV_800:
      Serial.print("ODR/800");
      break;
    default:
      Serial.print("Unknown");
      break;
  }
}

void halt(const __FlashStringHelper* message) {
  Serial.println(message);
  while (1) delay(10);
}

void setup() {
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  delay(100);  // Give time for power up

  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("Adafruit STHS34PF80 test!");

  // Initialize I2C_2 with the correct pins
  I2C_2.begin(SDA_2, SCL_2);
  I2C_2.setClock(400000);  // Set to 400kHz (fast mode)

  // Pass I2C_2 and address to begin()
  if (!sths.begin(0x5A, &I2C_2)) {
    halt(F("Could not find a valid STHS34PF80 sensor, check wiring!"));
  }

  Serial.println(F("STHS34PF80 Found on I2C_2!"));

  // Test all low-pass filter configurations
  Serial.println(F("\n--- Low-Pass Filter Tests ---"));

  // Test Motion LPF
  Serial.println(F("\nMotion LPF:"));
  if (!sths.setMotionLowPassFilter(STHS34PF80_LPF_ODR_DIV_9)) {
    halt(F("Failed to set Motion LPF"));
  }
  Serial.print(F("  Current setting: "));
  printLPFSetting(sths.getMotionLowPassFilter());
  Serial.println();

  // Test Motion+Presence LPF
  Serial.println(F("\nMotion+Presence LPF:"));
  if (!sths.setMotionPresenceLowPassFilter(STHS34PF80_LPF_ODR_DIV_20)) {
    halt(F("Failed to set Motion+Presence LPF"));
  }
  Serial.print(F("  Current setting: "));
  printLPFSetting(sths.getMotionPresenceLowPassFilter());
  Serial.println();

  // Test Presence LPF
  Serial.println(F("\nPresence LPF:"));
  if (!sths.setPresenceLowPassFilter(STHS34PF80_LPF_ODR_DIV_50)) {
    halt(F("Failed to set Presence LPF"));
  }
  Serial.print(F("  Current setting: "));
  printLPFSetting(sths.getPresenceLowPassFilter());
  Serial.println();

  // Test Temperature LPF
  Serial.println(F("\nTemperature LPF:"));
  if (!sths.setTemperatureLowPassFilter(STHS34PF80_LPF_ODR_DIV_100)) {
    halt(F("Failed to set Temperature LPF"));
  }
  Serial.print(F("  Current setting: "));
  printLPFSetting(sths.getTemperatureLowPassFilter());
  Serial.println();

  // Test Ambient Temperature Averaging
  Serial.println(F("\nAmbient Temperature Averaging:"));
  if (!sths.setAmbTempAveraging(STHS34PF80_AVG_T_8)) {
    halt(F("Failed to set Ambient Temperature Averaging"));
  }
  Serial.print(F("  Current: "));
  switch (sths.getAmbTempAveraging()) {
    case STHS34PF80_AVG_T_8: Serial.println(F("8 samples")); break;
    case STHS34PF80_AVG_T_4: Serial.println(F("4 samples")); break;
    case STHS34PF80_AVG_T_2: Serial.println(F("2 samples")); break;
    case STHS34PF80_AVG_T_1: Serial.println(F("1 sample")); break;
    default: Serial.println(F("Unknown")); break;
  }

  // Test Object Temperature Averaging
  Serial.println(F("\nObject Temperature Averaging:"));
  if (!sths.setObjAveraging(STHS34PF80_AVG_TMOS_32)) {
    halt(F("Failed to set Object Temperature Averaging"));
  }
  Serial.print(F("  Current: "));
  switch (sths.getObjAveraging()) {
    case STHS34PF80_AVG_TMOS_2: Serial.println(F("2 samples")); break;
    case STHS34PF80_AVG_TMOS_8: Serial.println(F("8 samples")); break;
    case STHS34PF80_AVG_TMOS_32: Serial.println(F("32 samples")); break;
    case STHS34PF80_AVG_TMOS_128: Serial.println(F("128 samples")); break;
    case STHS34PF80_AVG_TMOS_256: Serial.println(F("256 samples")); break;
    case STHS34PF80_AVG_TMOS_512: Serial.println(F("512 samples")); break;
    case STHS34PF80_AVG_TMOS_1024: Serial.println(F("1024 samples")); break;
    case STHS34PF80_AVG_TMOS_2048: Serial.println(F("2048 samples")); break;
    default: Serial.println(F("Unknown")); break;
  }

  // Test Wide Gain Mode
  Serial.println(F("\nWide Gain Mode:"));
  if (!sths.setWideGainMode(false)) {
    halt(F("Failed to set Wide Gain Mode"));
  }
  Serial.print(F("  Current: "));
  if (sths.getWideGainMode()) {
    Serial.println(F("Wide mode"));
  } else {
    Serial.println(F("Default gain mode"));
  }

  // Sensitivity (factory calibrated - read only)
  Serial.println(F("\nSensitivity:"));
  Serial.print(F("  Current: "));
  Serial.println(sths.getSensitivity());

  // Test Block Data Update
  Serial.println(F("\nBlock Data Update:"));
  if (!sths.setBlockDataUpdate(true)) {
    halt(F("Failed to set Block Data Update"));
  }
  Serial.print(F("  Current: "));
  if (sths.getBlockDataUpdate()) {
    Serial.println(F("Enabled"));
  } else {
    Serial.println(F("Disabled"));
  }

  // Set Output Data Rate to continuous mode (1 Hz)
  Serial.println(F("\nOutput Data Rate:"));
  if (!sths.setOutputDataRate(STHS34PF80_ODR_1_HZ)) {
    halt(F("Failed to set Output Data Rate"));
  }
  Serial.print(F("  Current: "));
  switch (sths.getOutputDataRate()) {
    case STHS34PF80_ODR_POWER_DOWN: Serial.println(F("Power-down")); break;
    case STHS34PF80_ODR_0_25_HZ: Serial.println(F("0.25 Hz")); break;
    case STHS34PF80_ODR_0_5_HZ: Serial.println(F("0.5 Hz")); break;
    case STHS34PF80_ODR_1_HZ: Serial.println(F("1 Hz")); break;
    case STHS34PF80_ODR_2_HZ: Serial.println(F("2 Hz")); break;
    case STHS34PF80_ODR_4_HZ: Serial.println(F("4 Hz")); break;
    case STHS34PF80_ODR_8_HZ: Serial.println(F("8 Hz")); break;
    case STHS34PF80_ODR_15_HZ: Serial.println(F("15 Hz")); break;
    case STHS34PF80_ODR_30_HZ: Serial.println(F("30 Hz")); break;
    default: Serial.println(F("Unknown")); break;
  }

  // Test CTRL2 functions
  Serial.println(F("\nEmbedded Function Page:"));
  if (!sths.enableEmbeddedFuncPage(false)) {
    halt(F("Failed to disable Embedded Function Page"));
  }

  // Only trigger one-shot if in power-down mode
  if (sths.getOutputDataRate() == STHS34PF80_ODR_POWER_DOWN) {
    Serial.println(F("\nTrigger One-shot:"));
    if (!sths.triggerOneshot()) {
      halt(F("Failed to trigger one-shot"));
    }
  } else {
    Serial.println(F("\nContinuous mode - no one-shot trigger needed"));
  }


  // Configure interrupts
  Serial.println(F("\nInterrupt Configuration:"));
  if (!sths.setIntPolarity(false)) {
    halt(F("Failed to set interrupt polarity"));
  }
  if (!sths.setIntOpenDrain(false)) {
    halt(F("Failed to set interrupt output type"));
  }
  if (!sths.setIntLatched(true)) {
    halt(F("Failed to set interrupt latched mode"));
  }

  // Enable interrupts for all three events (matches default)
  uint8_t mask = STHS34PF80_PRES_FLAG | STHS34PF80_MOT_FLAG | STHS34PF80_TAMB_SHOCK_FLAG;
  if (!sths.setIntMask(mask)) {
    halt(F("Failed to set interrupt mask"));
  }

  // Set interrupt signal to INT_OR (function flags) - matches default
  if (!sths.setIntSignal(STHS34PF80_INT_OR)) {
    halt(F("Failed to set interrupt signal"));
  }

  // Print current interrupt mask status
  Serial.print(F("  Current interrupt mask: 0x"));
  Serial.print(sths.getIntMask(), HEX);
  Serial.print(F(" ("));
  uint8_t currentMask = sths.getIntMask();
  if (currentMask & STHS34PF80_PRES_FLAG) Serial.print(F("PRES "));
  if (currentMask & STHS34PF80_MOT_FLAG) Serial.print(F("MOT "));
  if (currentMask & STHS34PF80_TAMB_SHOCK_FLAG) Serial.print(F("TAMB_SHOCK "));
  Serial.println(F(")"));

  // Print current interrupt signal setting
  Serial.print(F("  Current interrupt signal: "));
  switch (sths.getIntSignal()) {
    case STHS34PF80_INT_HIGH_Z:
      Serial.println(F("High-Z (disabled)"));
      break;
    case STHS34PF80_INT_DRDY:
      Serial.println(F("Data ready"));
      break;
    case STHS34PF80_INT_OR:
      Serial.println(F("INT_OR (function flags)"));
      break;
    default:
      Serial.println(F("Unknown"));
      break;
  }

  Serial.println(F("\nConfiguration complete!"));
}

void loop() {
  if (sths.isDataReady()) {
    Serial.print("Data ready! ");

    // Read temperature and detection data
    Serial.print("Amb: ");
    Serial.print(sths.readAmbientTemperature(), 2);
    Serial.print("°C, Obj: ");
    Serial.print(sths.readObjectTemperature());
    Serial.print(", Comp: ");
    Serial.print(sths.readCompensatedObjectTemperature());
    Serial.print(", Pres: ");
    Serial.print(sths.readPresence());
    Serial.print(", Mot: ");
    Serial.print(sths.readMotion());
    Serial.print(", Shock: ");
    Serial.print(sths.readTempShock());
    Serial.print(", ");

    // Check function status flags
    if (sths.isPresence()) {
      Serial.print("PRESENCE ");
    }
    if (sths.isMotion()) {
      Serial.print("MOTION ");
    }
    if (sths.isTempShock()) {
      Serial.print("TEMP_SHOCK ");
    }

    Serial.println();
  }
  delay(100);
}