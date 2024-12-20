#include "FED4.h"

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