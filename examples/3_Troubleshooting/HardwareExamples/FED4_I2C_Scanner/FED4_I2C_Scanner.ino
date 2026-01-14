#include <Wire.h>

// I2C Channel 1
#define SDA_PIN_1 8
#define SCL_PIN_1 9

// I2C Channel 2
#define SDA_PIN_2 20
#define SCL_PIN_2 19
#define LDO2 47

// Create two separate I2C instances
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== Dual I2C Scanner for ESP32 ===");
  Serial.println("Fixing timeout issues...");

  // Configure power pin for I2C_2
  pinMode(LDO2, OUTPUT);
  digitalWrite(LDO2, HIGH);
  Serial.println("Power enabled for I2C_2 (Pin 47 HIGH)");
  delay(1000);

  // Initialize I2C buses with slower clock speeds to avoid timeouts
  I2C_1.begin(SDA_PIN_1, SCL_PIN_1, 100000); // 100kHz
  I2C_2.begin(SDA_PIN_2, SCL_PIN_2, 100000); // 100kHz

  // Set timeout values
  I2C_1.setTimeout(1000); // 1 second timeout
  I2C_2.setTimeout(1000); // 1 second timeout

  Serial.println("I2C_1: SDA=8, SCL=9, 100kHz");
  Serial.println("I2C_2: SDA=20, SCL=19, 100kHz");
  Serial.println();
  
  delay(1000); // Give buses time to stabilize
}

void loop() {
  Serial.println("=== Starting I2C Scan ===");
  
  // Scan I2C_1
  scanI2C(I2C_1, "I2C_1 (pins 8,9)");
  
  Serial.println();
  
  // Scan I2C_2  
  scanI2C(I2C_2, "I2C_2 (pins 20,19)");
  
  Serial.println("=== Scan Complete ===");
  Serial.println();
  delay(10000); // Wait 10 seconds before next scan
}

void scanI2C(TwoWire &i2c, const char* busName) {
  byte error, address;
  int nDevices = 0;

  Serial.print("Scanning ");
  Serial.println(busName);
  Serial.println("------------------------");

  for (address = 8; address < 120; address++) { // Scan narrower range
    // Use a more reliable scanning method
    i2c.beginTransmission(address);
    error = i2c.endTransmission(true); // Send stop bit

    if (error == 0) {
      Serial.print("Device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      
      // Identify common devices
      identifyDevice(address);
      Serial.println(" !");
      nDevices++;
      
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
    
    delay(10); // Small delay between probes
  }
  
  if (nDevices == 0) {
    Serial.print("No I2C devices found on ");
    Serial.println(busName);
    Serial.println("Check: 1) Wiring 2) Pull-up resistors 3) Power supply");
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
      Serial.print(" (VL53L4CDV0DH/1 ToF)");
      break;
    case 0x36:
      Serial.print(" (MAX17048 Battery Monitor)");
      break;
    case 0x68:
      Serial.print(" (RTC)");
      break;
    case 0x76:
      Serial.print(" (BME680 Temp)");
      break;
    case 0x0C:
      Serial.print(" (MLX90393SLW Megnetometer)");
      break;
    case 0x5A:
      Serial.print(" (STHS34PF80TR Motion)");
      break;
    default:
      Serial.print(" (Unknown device)");
      break;
  }
}
