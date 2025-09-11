// This program reads data from a BME680 sensor with an ESP32 on custom I2C pins.

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define SDA_PIN 8
#define SCL_PIN 9

TwoWire I2C_BME = TwoWire(1);
Adafruit_BME680 bme(&I2C_BME); // Use I2C

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  I2C_BME.begin(SDA_PIN, SCL_PIN);

  Serial.println("BME680 Test on custom I2C pins");

  Serial.println(bme.begin(0x76));
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring on pins 8 & 9!");
    while (1);
  }

  // Sensor settings remain the same
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop() {
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading :(");
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
}
