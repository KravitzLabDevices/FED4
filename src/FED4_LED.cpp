#include "FED4.h"

void FED4::initializeLEDs()
{
    pinMode(RGB_STRIP_PIN, OUTPUT);
    pinMode(NEOPIXEL_PIN, OUTPUT);
    pixels.begin();
    noPix();
}

void FED4::bluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 20));
    pixels.show();
}

void FED4::dimBluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 1));
    pixels.show();
}

void FED4::greenPix()
{
    pixels.setPixelColor(0, pixels.Color(20, 0, 0));
    pixels.show();
}

void FED4::redPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 20, 0));
    pixels.show();
}

void FED4::purplePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 50, 50));
    pixels.show();
}

void FED4::noPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    digitalWrite(NEOPIXEL_PIN, LOW);
    digitalWrite(RGB_STRIP_PIN, LOW);
}