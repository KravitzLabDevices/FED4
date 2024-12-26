#include <Adafruit_NeoPixel.h>

#define TouchPad 5
#define TouchPad2 6
#define TouchPad3 1
#define PIN 35
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
	Serial.begin(115200);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH); // turn on the 3v rail for the RGB LED
  pixels.begin();
}

void loop()
{
	int t = touchRead(TouchPad);
	int t2 = touchRead(TouchPad2);
	int t3 = touchRead(TouchPad3);
	if (t < 40000)
	{
		Serial.println("");
	}
	if (t >= 50000)
	{
    Serial.println(t);
		pixels.setPixelColor(0, pixels.Color(150, 0, 0));
		pixels.show();
	}
	delay(100);
	if (t2 >= 50000)
	{ Serial.println(t2);
		pixels.setPixelColor(0, pixels.Color(0, 150, 0));
		pixels.show();
	}
	delay(100);
	if (t3 >= 50000)
	{ Serial.println(t3);
		pixels.setPixelColor(0, pixels.Color(0, 0, 150));
		pixels.show();
	}
	delay(100);

}

