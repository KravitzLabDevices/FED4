/*
 * FED4 I2C Scanner
 *
 * Scans the primary I2C bus (SDA=8, SCL=9).
 * The DS3231 RTC sits behind a TCA4307 isolator on the PSV2 rail,
 * so PSV2 must be enabled via the MCP23017 before the RTC appears (~ON active-low).
 *
 * Expected devices (see src/FED4_Pins.h):
 *   0x10  VEML7700 lux sensor
 *   0x19  LIS2DH12TR accelerometer
 *   0x20  MCP23017 GPIO expander
 *   0x29  VL53L1X time-of-flight sensor
 *   0x36  MAX17048 battery monitor
 *   0x68  DS3231 real-time clock (requires PSV2 / TCA4307)
 *   0x76  BME680 temperature, humidity, pressure, and gas sensor
 */

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <FED4_Pins.h>

Adafruit_MCP23X17 mcp;

void identifyDevice(byte address);
void scanI2C(const char *busName);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 I2C Scanner ===");
  Serial.println("Architecture: single main I2C + TCA4307 isolator for RTC");
  Serial.println();

  // Primary I2C bus (same as FED4::begin)
  Wire.begin(SDA, SCL, 100000); // 100kHz
  Wire.setTimeout(1000);

  Serial.println("I2C: SDA=8, SCL=9, 100kHz");

  if (!mcp.begin_I2C()) {
    Serial.println("Error initializing MCP23017 at 0x20.");
    Serial.println("Cannot enable PSV2 — RTC will not be visible.");
  } else {
    // Enable PSV2 so the TCA4307 isolator connects the RTC to the bus
    mcp.pinMode(EXP_PSV2_EN, OUTPUT);
    mcp.digitalWrite(EXP_PSV2_EN, LOW);
    Serial.println("PSV2 enabled (MCP pin 13 LOW, ~ON active-low) — RTC isolator powered");
  }

  Serial.println();
  delay(100); // Allow PSV2 / I2C isolator to stabilize
}

void loop() {
  Serial.println("=== Starting I2C Scan ===");
  scanI2C("Main I2C (SDA=8, SCL=9)");
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
    Serial.println("Check: 1) Wiring 2) Pull-ups 3) Power / PSV2 for RTC");
  } else {
    Serial.print("Found ");
    Serial.print(nDevices);
    Serial.print(" device(s) on ");
    Serial.println(busName);
  }
}

void identifyDevice(byte address) {
  switch (address) {
    case 0x10:    
      Serial.print(" (VEML7700-TT LUX)");
      break;
    case 0x19:
      Serial.print(" (LIS2DH12 Accelerometer)");
      break;
    case 0x20:
      Serial.print(" (MCP23017 GPIO Expander)");
      break;
    case 0x29:
      Serial.print(" (VL53L1X ToF)");
      break;
    case 0x36:
      Serial.print(" (MAX17048 Battery Monitor)");
      break;
    case 0x68:
      Serial.print(" (RTC via TCA4307)");
      break;
    case 0x76:
      Serial.print(" (BME680 Temp/Humidity/Gas)");
      break;
    default:
      Serial.print(" (Unknown device)");
      break;
  }
}
