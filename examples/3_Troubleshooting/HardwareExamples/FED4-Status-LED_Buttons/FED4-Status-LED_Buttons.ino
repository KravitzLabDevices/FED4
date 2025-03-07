#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

#define PIN 35
#define btn1 39
#define btn2 40
#define btn3 14
#define NUMPIXELS 1
#define LDO3 14
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

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
	pinMode(btn1, INPUT);
	pinMode(btn2, INPUT);
	pinMode(btn3, INPUT);

	pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}

void loop()
{
	pixels.clear();
	int button1_press = digitalRead(btn1);
	int button2_press = digitalRead(btn2);
	int button3_press = digitalRead(btn3);

	if (button1_press == HIGH)
	{
		Serial.println("button 1");
		pixels.setPixelColor(0, pixels.Color(150, 0, 0));
		pixels.show();
	}
	if (button2_press == HIGH)
	{
		Serial.println("button 2");
		pixels.setPixelColor(0, pixels.Color(0, 150, 0));
		pixels.show();
	}
	if (button3_press == HIGH)
	{
		Serial.println("button 3");
		pixels.setPixelColor(0, pixels.Color(0, 0, 150));
		pixels.show();
	}
	else
	{

		pixels.clear();
		pixels.show();
	}
}