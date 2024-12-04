#include "FED4.h"

void FED4::BluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 20));
    pixels.show();
}

void FED4::DimBluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 1));
    pixels.show();
}

void FED4::GreenPix()
{
    pixels.setPixelColor(0, pixels.Color(20, 0, 0));
    pixels.show();
}

void FED4::RedPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 20, 0));
    pixels.show();
}

void FED4::PurplePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 50, 50));
    pixels.show();
}

void FED4::NoPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    digitalWrite(NEOPIXEL_EN_PIN, LOW);
}