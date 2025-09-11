// This program scans the I2C bus to find connected devices on an ESP32.

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

#define XSHUT 1

void setup() {
  // Start serial communication for displaying the results
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for the serial port to connect
  }

  // Specific for ToF sensor to be discoverable
  // if (!mcp.begin_I2C()) {
  //   Serial.println("Error initializing MCP23017.");
  //   while (1)
  //     ;
  // }
  // mcp.pinMode(XSHUT, OUTPUT);
  // mcp.digitalWrite(XSHUT, HIGH);  // XSHUT must be pulled high for the sensor to be found

  Wire.begin();

  Serial.println("\nI2C Scanner");
  Serial.println("Scanning for I2C devices...");
}

void loop() {
  byte error, address;
  int nDevices;

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The I2C scanner uses the return value of Wire.endTransmission()
    // to see if a device acknowledged the transmission.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println(" !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("Scan complete.\n");
  }

  delay(5000); // Wait 5 seconds for the next scan
}
