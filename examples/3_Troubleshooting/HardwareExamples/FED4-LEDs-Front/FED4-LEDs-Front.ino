#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

#define PIN 36
#define btn1 14
#define btn2 39
#define btn3 40
#define NUMPIXELS 8    // Changed to 8 LEDs
#define LDO3 14
#define BRIGHTNESS 50

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(115200);
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH); 
  
  if (!mcp.begin_I2C())
  {
    Serial.println("Error.");
    while (1);
  }

  mcp.pinMode(LDO3, OUTPUT);
  mcp.digitalWrite(LDO3, HIGH);
  
  pinMode(btn1, INPUT);
  pinMode(btn2, INPUT);
  pinMode(btn3, INPUT);

  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
}

void loop()
{
  int button1_press = digitalRead(btn1);
  int button2_press = digitalRead(btn2);
  int button3_press = digitalRead(btn3);

  if (button1_press == HIGH)
  {
    Serial.println("Button 1 - Rainbow");
    rainbow(10, 1);
  }
  if (button2_press == HIGH)
  {
    Serial.println("Button 2 - Theatre Chase");
    theaterChase(pixels.Color(0, 255, 255), 100, 3, 5); // cyan
    theaterChase(pixels.Color(255, 0, 255), 100, 3, 5); // magenta
    theaterChase(pixels.Color(255, 255, 0), 100, 3, 5); // yellow
  }
  if (button3_press == HIGH)
  {
    Serial.println("Button 3 - Color Wipe");
    colorWipe(pixels.Color(255, 0, 0), 25); // red
    colorWipe(pixels.Color(0, 255, 0), 25); // green
    colorWipe(pixels.Color(0, 0, 255), 25); // blue
    colorWipe(pixels.Color(255, 255, 255), 25); // white
  }
  
  // Only clear if no buttons are pressed
  if (button1_press == LOW && button2_press == LOW && button3_press == LOW)
  {
    pixels.clear();
    pixels.show();
  }
  
  delay(10);
}

void colorWipe(uint32_t color, unsigned long wait)
{
  for (int i = 0; i < pixels.numPixels(); i++)
  {
    pixels.setPixelColor(i, color);
    pixels.show();
    delay(wait);
  }
}

void theaterChase(uint32_t color, unsigned long wait, unsigned int groupSize, unsigned int numChases)
{
  for (unsigned int chase = 0; chase < numChases; chase++)
  {
    for (unsigned int pos = 0; pos < groupSize; pos++)
    {
      pixels.clear();
      for (int i = pos; i < pixels.numPixels(); i += groupSize)
      {
        pixels.setPixelColor(i, color);
      }
      pixels.show();
      delay(wait);
    }
  }
}

void rainbow(unsigned long wait, unsigned int numLoops)
{
  for (unsigned int count = 0; count < numLoops; count++)
  {
    for (long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256)
    {
      for (int i = 0; i < pixels.numPixels(); i++)
      {
        int pixelHue = firstPixelHue + (i * 65536L / pixels.numPixels());
        pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
      }
      pixels.show();
      delay(wait);
    }
  }
}
