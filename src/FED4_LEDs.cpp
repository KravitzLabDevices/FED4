#include "FED4.h"

/********************************************************
 * FED4 LED Control Functions
 *
 * This file combines both NeoPixel (single LED) and LED Strip control.
 * All LED functions now support both traditional RGB values and
 * string-based color names.
 *
 * Available Colors:
 * - "red"    (255, 0, 0)
 * - "green"  (0, 255, 0)
 * - "blue"   (0, 0, 255)
 * - "white"  (255, 255, 255)
 * - "black"  (0, 0, 0)
 * - "yellow" (255, 255, 0)
 * - "purple" (128, 0, 128)
 * - "cyan"   (0, 255, 255)
 * - "orange" (255, 165, 0)
 *
 * Usage Examples:
 *
 * 1. Single NeoPixel:
 *    setPixColor("red");           // Default brightness (64)
 *    setPixColor("red", 255);      // Custom brightness
 *    setPixColor("blue", 128);     // Half brightness blue
 *
 * 2. LED Strip:
 *    setStripPixel(0, "green");    // First LED green
 *    leftLight("blue");            // Left poke blue
 *    centerLight("yellow");        // Center poke yellow
 *    rightLight("red");            // Right poke red
 *
 * Note: If an unrecognized color is provided, the LED(s)
 * will default to off.
 ********************************************************/

// STRIP FUNCTIONS

// Initialize the strip
bool FED4::initializeStrip()
{
    FastLED.addLeds<NEOPIXEL, RGB_STRIP_PIN>(strip_leds, NUM_STRIP_LEDS);
    setStripBrightness(50); // Default brightness
    
    // Test the strip by setting all pixels to red briefly
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Red);
    FastLED.show();
    delay(100);
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    FastLED.show();
    
    return true;
}

// Set the strip brightness
void FED4::setStripBrightness(uint8_t brightness)
{
    FastLED.setBrightness(brightness);
}

// Example usage:
// colorWipe("white", 10); // Fast white wipe
void FED4::colorWipe(const char* colorName, unsigned long wait)
{
    colorWipe(getColorFromString(colorName), wait);
}

// New overloaded function that takes uint32_t color value
void FED4::colorWipe(uint32_t color, unsigned long wait)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, color);
    FastLED.show();
    delay(wait * NUM_STRIP_LEDS); // Keep total time similar
}

// Example usage:
// Theater chase animations with different colors and timing:
// stripTheaterChase("white", 50, 3, 10);    // White chase, 50ms delay, groups of 3, 10 cycles
void FED4::stripTheaterChase(const char* colorName, unsigned long wait, unsigned int groupSize, unsigned int numChases)
{
    stripTheaterChase(getColorFromString(colorName), wait, groupSize, numChases);
}

// New overloaded function that takes uint32_t color value
void FED4::stripTheaterChase(uint32_t color, unsigned long wait, unsigned int groupSize, unsigned int numChases)
{
    for(int chase = 0; chase < numChases; chase++) {
        for(int q=0; q < groupSize; q++) {
            for(int i=0; i < NUM_STRIP_LEDS; i=i+groupSize) {
                strip_leds[i+q] = color;
            }
            FastLED.show();
            delay(wait);
            for(int i=0; i < NUM_STRIP_LEDS; i=i+groupSize) {
                strip_leds[i+q] = CRGB::Black;
            }
        }
    }
}

// Example usage:
// Rainbow animation with different speeds and repetitions:
// stripRainbow(50, 1);     // Rainbow animation with 50ms delay, 1 loop
// stripRainbow(100, 2);    // Rainbow animation with 100ms delay, 2 loops
// stripRainbow(25, 3);     // Fast rainbow animation with 25ms delay, 3 loops
void FED4::stripRainbow(unsigned long wait, unsigned int numLoops)
{
    for (unsigned int count = 0; count < numLoops; count++)
    {
        uint8_t hue = 0;
        for (int i = 0; i < 256; i++) {
            fill_rainbow(strip_leds, NUM_STRIP_LEDS, hue++, 255 / NUM_STRIP_LEDS);
            FastLED.show();
            delay(wait);
        }
    }
}

// Clear the strip
// Example usage:
// clearStrip();    // Clears the strip 
void FED4::clearStrip()
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    FastLED.show();
}

// Example usage:
// Set individual pixels to different colors:
// setStripPixel(0, "red");    // Set first pixel to red
// setStripPixel(3, "green");    // Set fourth pixel to green
// setStripPixel(7, "blue");    // Set eighth pixel to blue
void FED4::setStripPixel(uint8_t pixel, uint32_t color)
{
    if (pixel < NUM_STRIP_LEDS) {
        strip_leds[pixel] = color;
        FastLED.show();
    }
}

// Example usage:
// Light up left port with specific colors:
// leftLight("red");    // Set left port to red
// leftLight("green");    // Set left port to green
// leftLight("blue");    // Set left port to blue
void FED4::leftLight(uint32_t color)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    strip_leds[5] = color;
    strip_leds[6] = color;
    strip_leds[7] = color;
    FastLED.show();
}

// Example usage:
// Light up center port with specific colors:
// centerLight("red");    // Set center port to red
// centerLight("green");    // Set center port to green
// centerLight("blue");    // Set center port to blue
void FED4::centerLight(uint32_t color)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    strip_leds[3] = color;
    strip_leds[4] = color;
    FastLED.show();
}

// Example usage:
// Light up right port with specific colors:
// rightLight("red");    // Set right port to red
// rightLight("green");    // Set right port to green
// rightLight("blue");    // Set right port to blue
void FED4::rightLight(uint32_t color)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    strip_leds[0] = color;
    strip_leds[1] = color;
    strip_leds[2] = color;
    FastLED.show();
}

// Example usage:
// Set individual pixels using color names:
// setStripPixel(0, "red");     // Set first pixel to red
// setStripPixel(3, "green");   // Set fourth pixel to green
// setStripPixel(7, "blue");    // Set eighth pixel to blue
void FED4::setStripPixel(uint8_t pixel, const char *colorName)
{
    setStripPixel(pixel, getColorFromString(colorName));
}

// Example usage:
// Light up left port using color names:
// leftLight("red");     // Set left port to red
// leftLight("green");   // Set left port to green
// leftLight("blue");    // Set left port to blue
void FED4::leftLight(const char *colorName)
{
    leftLight(getColorFromString(colorName));
}

// Example usage:
// Light up center port using color names:
// centerLight("red");     // Set center port to red
// centerLight("green");   // Set center port to green
// centerLight("blue");    // Set center port to blue
void FED4::centerLight(const char *colorName)
{
    centerLight(getColorFromString(colorName));
}

// Example usage:
// Light up right port using color names:
// rightLight("red");     // Set right port to red
// rightLight("green");   // Set right port to green
// rightLight("blue");    // Set right port to blue
void FED4::rightLight(const char *colorName)
{
    rightLight(getColorFromString(colorName));
}

// Example usage:
// Initialize the NeoPixel:
// FED4.initializePixel();    // Sets up the NeoPixel with default brightness
bool FED4::initializePixel()
{
    pixels.begin(); // Initialize NeoPixel
    delay(10);
    pixels.clear();
    pixels.setBrightness(50); // Set a default brightness
    noPix();
    return true;
}

// Example usage:
// Set NeoPixel brightness:
// setPixBrightness(100);    // Set brightness to 100 (max 255)
// setPixBrightness(25);     // Set brightness to 25 (dimmer)
void FED4::setPixBrightness(uint8_t brightness)
{
    pixels.setBrightness(brightness);
}

// Example usage:
// Set NeoPixel color with RGB values and brightness:
// setPixColor(255, 0, 0, 100);    // Set to red with brightness 100
// setPixColor(0, 255, 0, 50);     // Set to green with brightness 50
void FED4::setPixColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    setPixBrightness(brightness);
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

// Example usage:
// Set NeoPixel to blue with custom brightness:
// bluePix(100);    // Set blue with brightness 100
// bluePix(50);     // Set blue with brightness 50
void FED4::bluePix(uint8_t brightness)
{
    setPixColor(0, 0, 255, brightness);
}

// Example usage:
// Set NeoPixel to green with custom brightness:
// greenPix(100);    // Set green with brightness 100
// greenPix(50);     // Set green with brightness 50
void FED4::greenPix(uint8_t brightness)
{
    setPixColor(0, 255, 0, brightness);
}

// Example usage:
// Set NeoPixel to red with custom brightness:
// redPix(100);    // Set red with brightness 100
// redPix(50);     // Set red with brightness 50
void FED4::redPix(uint8_t brightness)
{
    setPixColor(255, 0, 0, brightness);
}

// Example usage:
// Set NeoPixel to purple with custom brightness:
// purplePix(100);    // Set purple with brightness 100
// purplePix(50);     // Set purple with brightness 50
void FED4::purplePix(uint8_t brightness)
{
    setPixColor(128, 0, 128, brightness);
}

// Example usage:
// Set NeoPixel to yellow with custom brightness:
// yellowPix(100);    // Set yellow with brightness 100
// yellowPix(50);     // Set yellow with brightness 50
void FED4::yellowPix(uint8_t brightness)
{
    setPixColor(255, 255, 0, brightness);
}

// Example usage:
// Set NeoPixel to cyan with custom brightness:
// cyanPix(100);    // Set cyan with brightness 100
// cyanPix(50);     // Set cyan with brightness 50
void FED4::cyanPix(uint8_t brightness)
{
    setPixColor(0, 255, 255, brightness);
}

// Example usage:
// Set NeoPixel to white with custom brightness:
// whitePix(100);    // Set white with brightness 100
// whitePix(50);     // Set white with brightness 50
void FED4::whitePix(uint8_t brightness)
{
    setPixColor(255, 255, 255, brightness);
}

// Example usage:
// Set NeoPixel to orange with custom brightness:
// orangePix(100);    // Set orange with brightness 100
// orangePix(50);     // Set orange with brightness 50
void FED4::orangePix(uint8_t brightness)
{
    setPixColor(255, 165, 0, brightness);
}

// Example usage:
// Turn off NeoPixel:
// noPix();    // Sets pixel to black (off)
void FED4::noPix()
{
    setPixColor(0, 0, 0, 255);
}

// Example usage:
// Set NeoPixel color using color name and brightness:
// setPixColor("red", 100);      // Set to red with brightness 100
// setPixColor("green", 50);     // Set to green with brightness 50
void FED4::setPixColor(const char *colorName, uint8_t brightness)
{
    uint32_t color = getColorFromString(colorName);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    setPixColor(r, g, b, brightness);
}

// Example usage:
// Convert color name to RGB value:
// uint32_t redColor = getColorFromString("red");       // Get red color value
// uint32_t blueColor = getColorFromString("blue");     // Get blue color value
uint32_t FED4::getColorFromString(const char *colorName)
{
    if (strcasecmp(colorName, "red") == 0)
        return CRGB::Red;
    if (strcasecmp(colorName, "green") == 0)
        return CRGB::Green;
    if (strcasecmp(colorName, "blue") == 0)
        return CRGB::Blue;
    if (strcasecmp(colorName, "white") == 0)
        return CRGB::White;
    if (strcasecmp(colorName, "black") == 0)
        return CRGB::Black;
    if (strcasecmp(colorName, "yellow") == 0)
        return CRGB::Yellow;
    if (strcasecmp(colorName, "purple") == 0)
        return CRGB::Purple;
    if (strcasecmp(colorName, "cyan") == 0)
        return CRGB::Cyan;
    if (strcasecmp(colorName, "orange") == 0)
        return CRGB::Orange;
    return CRGB::Black; // Default to off if color not recognized
}