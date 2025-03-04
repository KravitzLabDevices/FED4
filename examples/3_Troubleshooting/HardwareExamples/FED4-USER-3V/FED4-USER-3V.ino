/*
To turn on the 3v power for the user controllable pins to the left of the motor pins.
*/

#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;
#define DEVAL 3000
#define LDO3 14

void setup()
{
  Serial.begin(115200);
  pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
	if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1)
			;
	}

	mcp.pinMode(LDO3, OUTPUT);
	mcp.digitalWrite(LDO3, HIGH);
}

void loop()
{
	mcp.digitalWrite(LDO3, LOW);
  Serial.println("low");
	delay(DEVAL);
	mcp.digitalWrite(LDO3, HIGH);
    Serial.println("high");
	delay(DEVAL);
}
