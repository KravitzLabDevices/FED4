/*
To turn on the 3v power for the user controllable pins to the left of the motor pins.
*/

#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;
#define DEVAL 1000
#define LDO3 14

void setup()
{
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
	delay(DEVAL);
	mcp.digitalWrite(LDO3, HIGH);
	delay(DEVAL);
}
