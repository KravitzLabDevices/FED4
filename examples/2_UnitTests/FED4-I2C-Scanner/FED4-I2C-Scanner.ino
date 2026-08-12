/*
 * FED4 I2C Scanner
 *
 * Scans the primary I2C bus (SDA=8, SCL=9).
 * RTC (DS3231) and all peripherals are on the main always-on bus (no PSV2 / TCA4307).
 *
 * Expected devices (see src/FED4_Pins.h, examples/3_Submodules/README.md):
 *   0x10  VEML7700 lux sensor
 *   0x19  LIS2DH12TR accelerometer
 *   0x20  MCP23017 GPIO expander
 *   0x29  VL53L1X time-of-flight sensor
 *   0x36  MAX17048 battery monitor
 *   0x42  SeeedStudio submodule (when connected)
 *   0x68  DS3231 real-time clock
 *   0x76  BME680 temperature, humidity, pressure, and gas sensor
 */

#include <Wire.h>
#include <FED4_Pins.h>

static const uint8_t EXPECTED_ADDRS[] = {
    I2C_ADDR_LIGHT,
    I2C_ADDR_ACCEL,
    I2C_ADDR_MCP23017,
    I2C_ADDR_TOF,
    I2C_ADDR_MAX17048,
    0x42,
    I2C_ADDR_RTC,
    I2C_ADDR_BME680,
};
static const size_t NUM_EXPECTED = sizeof(EXPECTED_ADDRS) / sizeof(EXPECTED_ADDRS[0]);

static const size_t MAX_FOUND = 16;
static uint8_t foundAddrs[MAX_FOUND];
static size_t numFound = 0;

void identifyDevice(byte address);
bool isExpectedAddress(byte address);
void listOtherAddresses();
void scanI2C(const char *busName);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 I2C Scanner ===");
  Serial.println("Main I2C bus (SDA=8, SCL=9) — no PSV2 / isolator required");
  Serial.println();

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  Serial.println("I2C: SDA=8, SCL=9, 100kHz");
  Serial.println();
}

void loop() {
  numFound = 0;

  Serial.println("=== Starting I2C Scan ===");
  scanI2C("Main I2C (SDA=8, SCL=9)");
  listOtherAddresses();
  Serial.println("=== Scan Complete ===");
  Serial.println();
  delay(10000);
}

void scanI2C(const char *busName) {
  byte error, address;
  int nDevices = 0;

  Serial.print("Scanning ");
  Serial.println(busName);
  Serial.println("------------------------");

  for (address = 0x08; address < 0x78; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission(true);

    if (error == 0) {
      if (numFound < MAX_FOUND) {
        foundAddrs[numFound++] = address;
      }

      Serial.print("Device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      identifyDevice(address);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }

    delay(10);
  }

  if (nDevices == 0) {
    Serial.print("No I2C devices found on ");
    Serial.println(busName);
    Serial.println("Check: 1) Wiring 2) Pull-ups 3) Power");
  } else {
    Serial.print("Found ");
    Serial.print(nDevices);
    Serial.print(" device(s) on ");
    Serial.println(busName);
  }
}

bool isExpectedAddress(byte address) {
  for (size_t i = 0; i < NUM_EXPECTED; i++) {
    if (EXPECTED_ADDRS[i] == address) {
      return true;
    }
  }
  return false;
}

void listOtherAddresses() {
  size_t otherCount = 0;

  for (size_t i = 0; i < numFound; i++) {
    if (!isExpectedAddress(foundAddrs[i])) {
      otherCount++;
    }
  }

  if (otherCount == 0) {
    Serial.println("Other addresses: none");
    return;
  }

  Serial.print("Other addresses (");
  Serial.print(otherCount);
  Serial.println("):");
  for (size_t i = 0; i < numFound; i++) {
    const byte address = foundAddrs[i];
    if (!isExpectedAddress(address)) {
      Serial.print("  0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
}

void identifyDevice(byte address) {
  switch (address) {
    case I2C_ADDR_LIGHT:
      Serial.print(" (VEML7700-TT LUX)");
      break;
    case I2C_ADDR_ACCEL:
      Serial.print(" (LIS2DH12 Accelerometer)");
      break;
    case I2C_ADDR_MCP23017:
      Serial.print(" (MCP23017 GPIO Expander)");
      break;
    case I2C_ADDR_TOF:
      Serial.print(" (VL53L1X ToF)");
      break;
    case I2C_ADDR_MAX17048:
      Serial.print(" (MAX17048 Battery Monitor)");
      break;
    case 0x42:
      Serial.print(" (SeeedStudio Submodule)");
      break;
    case I2C_ADDR_RTC:
      Serial.print(" (DS3231 RTC)");
      break;
    case I2C_ADDR_BME680:
      Serial.print(" (BME680 Temp/Humidity/Gas)");
      break;
    default:
      Serial.print(" (Other device)");
      break;
  }
}
