/*
 * FED4 Accelerometer Test (Adafruit LIS3DH / LIS2DH12TR)
 *
 * Reads acceleration from the LIS2DH12TR on the primary I2C bus.
 * The Adafruit_LIS3DH library is used (register-compatible with LIS2DH12).
 *
 * Pins / address (see src/FED4_Pins.h, src/FED4_Accel.cpp):
 *   SDA=8, SCL=9, 100kHz
 *   I2C address 0x19
 *
 * Axis remap (device orientation, screen facing you):
 *   Device X (left/right)  = -Chip Y
 *   Device Y (forward/back) =  Chip X
 *   Device Z (up/down)      =  Chip Z
 */

#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <FED4_Pins.h>

Adafruit_LIS3DH accel = Adafruit_LIS3DH();

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Accelerometer Test ===");
  Serial.println("LIS2DH12TR on main I2C (Adafruit_LIS3DH driver)");

  Wire.begin(SDA, SCL, 100000); // 100kHz — match FED4 bus
  Wire.setTimeout(1000);

  if (!accel.begin(I2C_ADDR_ACCEL)) {
    Serial.println("Could not find accelerometer at 0x19.");
    Serial.println("Check wiring / power. Freezing...");
    while (1) delay(10);
  }

  // Defaults match FED4::initializeAccel()
  accel.setRange(LIS3DH_RANGE_2_G);
  accel.setDataRate(LIS3DH_DATARATE_50_HZ);
  accel.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);

  Serial.println("Accelerometer found!");
  Serial.println();
}

void loop() {
  sensors_event_t event;
  if (!accel.getEvent(&event)) {
    Serial.println("Failed to read accel event");
    delay(500);
    return;
  }

  // Device-oriented axes (same remap as FED4::readAccel)
  float x = -event.acceleration.y;
  float y = event.acceleration.x;
  float z = event.acceleration.z;

  // Convert m/s^2 -> g
  const float g = 9.80665f;
  Serial.print("X: ");
  Serial.print(x / g, 3);
  Serial.print(" g, Y: ");
  Serial.print(y / g, 3);
  Serial.print(" g, Z: ");
  Serial.print(z / g, 3);
  Serial.println(" g");

  delay(500);
}
