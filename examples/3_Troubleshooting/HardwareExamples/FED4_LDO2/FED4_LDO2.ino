/*
To turn on the 3v power for the user controllable pins to the left of the motor pins.
*/

#include <Adafruit_MCP23X17.h>
#define DEVAL 3000
#define LDO2 47

void setup()
{
  Serial.begin(115200);
  pinMode(LDO2, OUTPUT);
  digitalWrite(LDO2,HIGH);
}

void loop()
{
  Serial.println("LDO2 High");
	// digitalWrite(LDO2, LOW);
  // Serial.println("low");
	// delay(DEVAL);
	// digitalWrite(LDO2, HIGH);
  // Serial.println("high");
	// delay(DEVAL);
  delay(3000);
}
