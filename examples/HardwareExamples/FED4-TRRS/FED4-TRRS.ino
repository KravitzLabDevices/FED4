#include <Adafruit_NeoPixel.h>
#define PIN 35
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
	Serial.begin(115200);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH); // turn on the 3v rail for the RGB LED
	pinMode(2, INPUT);
	pinMode(3, INPUT);
	pinMode(4, INPUT_PULLUP);

	pixels.begin();
}

void loop()
{
	pixels.clear();
	float trrs1 = analogRead(2);
	float trrs2 = analogRead(3);
	float trrs3 = analogRead(4);

	Serial.print(trrs1);
	Serial.print(" / ");
	Serial.print(trrs2);
	Serial.print(" / ");
	Serial.println(trrs3);

	if (trrs3 == 0)
	{ // TRRS 3 pulls to ground when plugged in and can be used to detect if there is a cable plugged in.

		pixels.setPixelColor(0, pixels.Color(150, 0, 0));

		pixels.show();
	}
	else
	{
		pixels.clear();
		pixels.show();
	}
	delay(100);
}