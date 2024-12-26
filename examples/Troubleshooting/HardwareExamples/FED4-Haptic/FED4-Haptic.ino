#include <Adafruit_MCP23X17.h>

#define haptic 8

Adafruit_MCP23X17 mcp;

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
	delay(500);
	mcp.pinMode(haptic, OUTPUT);
	mcp.digitalWrite(haptic, LOW);
}

void loop()
{

	Serial.println("on");
	mcp.digitalWrite(haptic, HIGH);
	delay(1000);
	Serial.println("off");
	mcp.digitalWrite(haptic, LOW);
	delay(1000);
}