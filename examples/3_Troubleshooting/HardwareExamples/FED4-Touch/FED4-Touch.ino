#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

#define TouchPad 5
#define TouchPad2 6
#define TouchPad3 1
#define PIN 35
#define NUMPIXELS 1
#define LDO3 14
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
	Serial.begin(115200);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH); // turn on the 3v rail for the RGB LED
	if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1);
	}

	mcp.pinMode(LDO3, OUTPUT);
	mcp.digitalWrite(LDO3, HIGH);

  pixels.begin();
}

void loop()
{
	int t1 = touchRead(TouchPad);
	int t2 = touchRead(TouchPad2);
	int t3 = touchRead(TouchPad3);
	if (t1 >= 110000)
	{
    Serial.println(t1);
		pixels.setPixelColor(0, pixels.Color(150, 0, 0));
		pixels.show();
	}
	delay(100);
	if (t2 >= 110000)
	{ Serial.println(t2);
		pixels.setPixelColor(0, pixels.Color(0, 150, 0));
		pixels.show();
	}
	delay(100);
	if (t3 >= 81000)
	{ Serial.println(t3);
		pixels.setPixelColor(0, pixels.Color(0, 0, 150));
		pixels.show();
	}
	delay(100);

}

