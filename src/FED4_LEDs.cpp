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
 *    fed.setPixColor("red");           // Default brightness (64)
 *    fed.setPixColor("red", 255);      // Custom brightness
 *    fed.setPixColor("blue", 128);     // Half brightness blue
 *
 * 2. LED Strip:
 *    fed.setStripPixel(0, "green");    // First LED green
 *    fed.leftLight("blue");            // Left poke blue
 *    fed.centerLight("yellow");        // Center poke yellow
 *    fed.rightLight("red");            // Right poke red
 *
 * Note: If an unrecognized color is provided, the LED(s)
 * will default to off.
 ********************************************************/

// STRIP FUNCTIONS
bool FED4::initializeStrip()
{
    strip.begin();
    strip.setBrightness(50); // Default brightness
    strip.clear();
    strip.show();
    return true;
}

void FED4::setStripBrightness(uint8_t brightness)
{
    strip.setBrightness(brightness);
}

void FED4::colorWipe(uint32_t color, unsigned long wait)
{
    for (unsigned int i = 0; i < strip.numPixels(); i++)
    {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}

void FED4::stripTheaterChase(uint32_t color, unsigned long wait, unsigned int groupSize, unsigned int numChases)
{
    for (unsigned int chase = 0; chase < numChases; chase++)
    {
        for (unsigned int pos = 0; pos < groupSize; pos++)
        {
            strip.clear();
            for (unsigned int i = pos; i < strip.numPixels(); i += groupSize)
            {
                strip.setPixelColor(i, color);
            }
            strip.show();
            delay(wait);
        }
    }
}

void FED4::stripRainbow(unsigned long wait, unsigned int numLoops)
{
    for (unsigned int count = 0; count < numLoops; count++)
    {
        for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256)
        {
            for (int i = 0; i < strip.numPixels(); i++)
            {
                int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
                strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
            }
            strip.show();
            delay(wait);
        }
    }
}

void FED4::clearStrip()
{
    strip.clear();
    strip.show();
}

void FED4::setStripPixel(uint8_t pixel, uint32_t color)
{
    strip.setPixelColor(pixel, color);
    strip.show();
}

void FED4::leftLight(uint32_t color)
{
    strip.clear();
    strip.setPixelColor(5, color);
    strip.setPixelColor(6, color);
    strip.setPixelColor(7, color);
    strip.show();
}

void FED4::centerLight(uint32_t color)
{
    strip.clear();
    strip.setPixelColor(3, color);
    strip.setPixelColor(4, color);
    strip.show();
}

void FED4::rightLight(uint32_t color)
{
    strip.clear();
    strip.setPixelColor(0, color);
    strip.setPixelColor(1, color);
    strip.setPixelColor(2, color);
    strip.show();
}

void FED4::setStripPixel(uint8_t pixel, const char *colorName)
{
    setStripPixel(pixel, getColorFromString(colorName));
}

void FED4::leftLight(const char *colorName)
{
    leftLight(getColorFromString(colorName));
}

void FED4::centerLight(const char *colorName)
{
    centerLight(getColorFromString(colorName));
}

void FED4::rightLight(const char *colorName)
{
    rightLight(getColorFromString(colorName));
}

// PIXEL FUNCTIONS
bool FED4::initializePixel()
{
    pixels.begin(); // Initialize NeoPixel
    pixels.clear();
    pixels.setBrightness(50); // Set a default brightness
    noPix();
    return true;
}

void FED4::setPixBrightness(uint8_t brightness)
{
    pixels.setBrightness(brightness);
}

void FED4::setPixColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    setPixBrightness(brightness);
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

// Colors with brightness control
void FED4::bluePix(uint8_t brightness)
{
    setPixColor(0, 0, 255, brightness);
}

void FED4::greenPix(uint8_t brightness)
{
    setPixColor(0, 255, 0, brightness);
}

void FED4::redPix(uint8_t brightness)
{
    setPixColor(255, 0, 0, brightness);
}

void FED4::purplePix(uint8_t brightness)
{
    setPixColor(128, 0, 128, brightness);
}

void FED4::yellowPix(uint8_t brightness)
{
    setPixColor(255, 255, 0, brightness);
}

void FED4::cyanPix(uint8_t brightness)
{
    setPixColor(0, 255, 255, brightness);
}

void FED4::whitePix(uint8_t brightness)
{
    setPixColor(255, 255, 255, brightness);
}

void FED4::orangePix(uint8_t brightness)
{
    setPixColor(255, 165, 0, brightness);
}

void FED4::noPix()
{
    setPixColor(0, 0, 0, 255);
}

void FED4::setPixColor(const char *colorName, uint8_t brightness)
{
    uint32_t color = getColorFromString(colorName);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    setPixColor(r, g, b, brightness);
}

// SHARED FUNCTIONS
uint32_t FED4::getColorFromString(const char *colorName)
{
    if (strcasecmp(colorName, "red") == 0)
        return strip.Color(255, 0, 0);
    if (strcasecmp(colorName, "green") == 0)
        return strip.Color(0, 255, 0);
    if (strcasecmp(colorName, "blue") == 0)
        return strip.Color(0, 0, 255);
    if (strcasecmp(colorName, "white") == 0)
        return strip.Color(255, 255, 255);
    if (strcasecmp(colorName, "black") == 0)
        return strip.Color(0, 0, 0);
    if (strcasecmp(colorName, "yellow") == 0)
        return strip.Color(255, 255, 0);
    if (strcasecmp(colorName, "purple") == 0)
        return strip.Color(128, 0, 128);
    if (strcasecmp(colorName, "cyan") == 0)
        return strip.Color(0, 255, 255);
    if (strcasecmp(colorName, "orange") == 0)
        return strip.Color(255, 165, 0);
    return strip.Color(0, 0, 0); // Default to off if color not recognized
}