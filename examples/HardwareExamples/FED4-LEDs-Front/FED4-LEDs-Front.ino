/*
Any Neopixel library should work. Main thing to add is the

pinMode(47, OUTPUT);
digitalWrite(47, HIGH);

to your setup function as this provides power to the LEDs.

otherwise, just a Data pin of 36 and num leds of 8 should be all you need.


*/

#include <FastLED_NeoPixel.h>
#define DATA_PIN 36	  // Which pin on the Arduino is connected to the LEDs?
#define NUM_LEDS 8	  // How many LEDs are there?
#define BRIGHTNESS 50 // LED brightness, 0 (min) to 255 (max)
#define BLINK_TIME 1000
CRGB leds[NUM_LEDS];
FastLED_NeoPixel<NUM_LEDS, DATA_PIN, NEO_GRB> strip; // <- FastLED NeoPixel version

void setup()
{
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
	strip.begin(); // initialize strip (required!)
	strip.setBrightness(BRIGHTNESS);
}

void loop()
{
	colorWipe(strip.Color(255, 0, 0), 25); // red
	colorWipe(strip.Color(0, 255, 0), 25); // green
	colorWipe(strip.Color(0, 0, 255), 25); // blue

	colorWipe(strip.Color(255, 255, 255), 25); // white

	theaterChase(strip.Color(0, 255, 255), 100, 3, 5); // cyan
	theaterChase(strip.Color(255, 0, 255), 100, 3, 5); // magenta
	theaterChase(strip.Color(255, 255, 0), 100, 3, 5); // yellow

	rainbow(10, 3);

	blank(1000);

	// turn on a specific LED. 0 is the left poke LED, 7 is the right poke LED. 1-6 are the LEDs in the row.
	strip.setPixelColor(7, strip.Color(0, 0, 255)); // set pixel 0 to blue
	strip.show();
	delay(BLINK_TIME);

	strip.setPixelColor(7, strip.Color(0, 0, 0)); // set pixel 0 to black
	strip.show();
	delay(BLINK_TIME);
}

/*
 * Fills a strip with a specific color, starting at 0 and continuing
 * until the entire strip is filled. Takes two arguments:
 *
 *     1. the color to use in the fill
 *     2. the amount of time to wait after writing each LED
 */
void colorWipe(uint32_t color, unsigned long wait)
{
	for (unsigned int i = 0; i < strip.numPixels(); i++)
	{
		strip.setPixelColor(i, color);
		strip.show();
		delay(wait);
	}
}

/*
 * Runs a marquee style "chase" sequence. Takes three arguments:
 *
 *     1. the color to use in the chase
 *     2. the amount of time to wait between frames
 *     3. the number of LEDs in each 'chase' group
 *     3. the number of chases sequences to perform
 */
void theaterChase(uint32_t color, unsigned long wait, unsigned int groupSize, unsigned int numChases)
{
	for (unsigned int chase = 0; chase < numChases; chase++)
	{
		for (unsigned int pos = 0; pos < groupSize; pos++)
		{
			strip.clear(); // turn off all LEDs
			for (unsigned int i = pos; i < strip.numPixels(); i += groupSize)
			{
				strip.setPixelColor(i, color); // turn on the current group
			}
			strip.show();
			delay(wait);
		}
	}
}

/*
 * Simple rainbow animation, iterating through all 8-bit hues. LED color changes
 * based on position in the strip. Takes two arguments:
 *
 *     1. the amount of time to wait between frames
 *     2. the number of rainbows to loop through
 */
void rainbow(unsigned long wait, unsigned int numLoops)
{
	for (unsigned int count = 0; count < numLoops; count++)
	{
		// iterate through all 8-bit hues, using 16-bit values for granularity
		for (unsigned long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256)
		{
			for (unsigned int i = 0; i < strip.numPixels(); i++)
			{
				unsigned long pixelHue = firstPixelHue + (i * 65536UL / strip.numPixels()); // vary LED hue based on position
				strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));			// assign color, using gamma curve for a more natural look
			}
			strip.show();
			delay(wait);
		}
	}
}

/*
 * Blanks the LEDs and waits for a short time.
 */
void blank(unsigned long wait)
{
	strip.clear();
	strip.show();
	delay(wait);
}
