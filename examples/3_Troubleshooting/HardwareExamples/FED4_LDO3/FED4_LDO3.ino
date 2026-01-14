#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

#define LDO3 14

void setup()

{

	Serial.begin(115200);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH); 
  if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1);
  }

	mcp.pinMode(LDO3, OUTPUT);
	mcp.digitalWrite(LDO3, HIGH);
  
}

void loop()
{
  
	Serial.println("Hello World!");
	delay(1000);
}