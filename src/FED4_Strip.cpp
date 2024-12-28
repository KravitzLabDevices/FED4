#include "FED4.h"

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

void FED4::leftLight(uint32_t color) //can these accept String arguments like "red", "blue", "green", etc?
{
    strip.clear();
    strip.setPixelColor(5, color);
    strip.setPixelColor(6, color);
    strip.setPixelColor(7, color);
    strip.show();
}

void FED4::centerLight(uint32_t color) //can these accept String arguments like "red", "blue", "green", etc?
{
    strip.clear();
    strip.setPixelColor(3, color);
    strip.setPixelColor(4, color);
    strip.show();
}

void FED4::rightLight(uint32_t color) //can these accept String arguments like "red", "blue", "green", etc?
{
    strip.clear();
    strip.setPixelColor(0, color);
    strip.setPixelColor(1, color);
    strip.setPixelColor(2, color);
    strip.show();
}



