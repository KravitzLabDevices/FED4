/*
 * FED4 Temperature / BME680 Test
 *
 * Reads temperature, humidity, pressure, and gas from the BME680
 * on the primary I2C bus.
 *
 * Hardware (see src/FED4_Pins.h, src/FED4_Begin.cpp):
 *   SDA=8, SCL=9, 100kHz
 *   I2C address 0x76 (SDO=LOW)
 *   Always-on 3.3V rail — no extra power enable needed
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define I2C_ADDR_BME680 0x76

Adafruit_BME680 bme; // Uses default Wire bus

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 BME680 Test ===");
  Serial.println("Temp/Humidity/Pressure/Gas on main I2C");

  Wire.begin(SDA_PIN, SCL_PIN, 100000); // Match FED4::begin()
  Wire.setTimeout(1000);

  if (!bme.begin(I2C_ADDR_BME680, &Wire)) {
    Serial.println("Could not find BME680 at 0x76.");
    Serial.println("Check wiring on SDA=8, SCL=9. Freezing...");
    while (1) delay(10);
  }

  // Oversampling / filter (reasonable defaults for troubleshooting)
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms

  Serial.println("BME680 found!");
  Serial.println();
}

void loop() {
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading");
    delay(1000);
    return;
  }

  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas Resistance = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.println();
  delay(2000);
}
