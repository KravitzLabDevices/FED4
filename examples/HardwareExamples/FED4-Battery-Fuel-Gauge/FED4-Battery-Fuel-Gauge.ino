#include "Adafruit_MAX1704X.h"
#include <Adafruit_MCP23X17.h>
Adafruit_MAX17048 maxlipo;
Adafruit_MCP23X17 mcp;
void setup() {
  Serial.begin(115200);
   pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  while (!Serial) delay(10);    // wait until serial monitor opens
   if (!mcp.begin_I2C()) {      // for some very strange reason the GPIO Expander needs turned on otherwise it fails to find the fuel gage. 
    Serial.println("Error.");   // this is a library issue that is documented.
    while (1);
  }

  Serial.println(F("\nAdafruit MAX17048 simple demo"));

    while (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!\n"));
    delay(5000);
  }
  Serial.print(F("Found MAX17048"));
  Serial.print(F(" with Chip ID: 0x")); 
  Serial.println(maxlipo.getChipID(), HEX);
}

void loop() {
  float cellVoltage = maxlipo.cellVoltage();
  if (isnan(cellVoltage)) {
    Serial.println("Failed to read cell voltage, check battery is connected!");
    delay(2000);
    return;
  }
  Serial.print(F("Batt Voltage: ")); Serial.print(cellVoltage, 3); Serial.println(" V");
  Serial.print(F("Batt Percent: ")); Serial.print(maxlipo.cellPercent(), 1); Serial.println(" %");
  Serial.println();

  delay(2000);  // dont query too often!
}
