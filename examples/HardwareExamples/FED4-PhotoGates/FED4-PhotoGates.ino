#include <Adafruit_MCP23X17.h>

#define PG1 12
#define PG2 13
#define PG3 0
#define PG4 11

Adafruit_MCP23X17 mcp;

void setup()
{
	Serial.begin(115200);
	while (!Serial)
		;
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);

	if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1)
			;
	}

	mcp.pinMode(PG1, INPUT_PULLUP);
	mcp.pinMode(PG2, INPUT_PULLUP);
	mcp.pinMode(PG3, INPUT_PULLUP);
	mcp.pinMode(PG4, INPUT_PULLUP);
}

void loop()
{

	int PG1Read = mcp.digitalRead(PG1);
	int PG2Read = mcp.digitalRead(PG2);
	int PG3Read = mcp.digitalRead(PG3);
	int PG4Read = mcp.digitalRead(PG4);

	if (PG1Read == LOW)
	{
		Serial.println("Center Nose Poke");
		delay(300);
	}
	if (PG2Read == LOW)
	{
		Serial.println("Right Nose Poke");
		delay(300);
	}
	if (PG3Read == LOW)
	{
		Serial.println("Left Nose Poke");
		delay(300);
	}
	if (PG4Read == LOW)
	{
		Serial.println("Pellet Detector");
		delay(300);
	}
}