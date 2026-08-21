#include "FED4.h"

/********************************************************
 * FED4 LED Control Functions
 *
 * Two independent LED systems:
 *
 * 1. RGB LED Strip (WS2812B, 8 LEDs, PSV3 rail, FastLED)
 *    Full RGB color control via string names or uint32_t values.
 *    Available colors: "red", "green", "blue", "white", "black",
 *                      "yellow", "purple", "cyan", "orange"
 *
 *    leftLight("blue");            // Left poke LEDs blue
 *    leftLight("blue", 100);       // Left poke LEDs blue, brightness 100
 *    centerLight("yellow");        // Center poke LEDs yellow
 *    rightLight("red", 200);       // Right poke LEDs red, brightness 200
 *    setStripPixel(0, "green");    // Individual LED green
 *    colorWipe("white", 10);       // Wipe animation
 *
 *  Available Colors:
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
 * 2. Status LED (single red LED, GPIO STATUS_LED, always-on 3.3V rail)
 *    Brightness-only control via PWM (0–255 duty cycle).
 *
 *    redPix(5);     // Dim red indicator
 *    redPix(128);   // Half brightness
 *    noPix();       // Off
 *
 * Note: If an unrecognized color name is passed to a strip function
 * the LED(s) default to off.
 ********************************************************/

// ── RGB LED STRIP (WS2812B, 8 LEDs, PSV3 rail) ──────────────────────────────

// Initialize the strip
bool FED4::initializeStrip()
{
    // Ensure PSV3 rail (front LED strip) is enabled before initializing
    PSV3_ON();
    delay(2);

    // Explicit WS2812B + GRB order for front strip reliability
    FastLED.addLeds<WS2812B, RGB_STRIP, GRB>(strip_leds, NUM_STRIP_LEDS);
    setStripBrightness(50); // Default brightness
    
    // Test the strip by setting all pixels to red briefly
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Red);
    FastLED.show();
    delay(1);
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

// Example usage:
// Random motion animation with different motion strengths:
// randomMotion(-1.0, "white", 50, 10000);    // 100% left motion, white dots, 50ms delay, 10 seconds duration
// randomMotion(1.0, "red", 30, 5000);       // 100% right motion, red dots, 30ms delay, 5 seconds duration
// randomMotion(0.0, "blue", 40, 8000);      // Random direction, blue dots, 40ms delay, 8 seconds duration
// randomMotion(0.5, "green", 50, 10000);     // 75% right bias, green dots, 50ms delay, 10 seconds duration
// randomMotion(-0.5, "yellow", 50, 10000);   // 75% left bias, yellow dots, 50ms delay, 10 seconds duration
void FED4::randomMotion(float motionStrength, const char *colorName, unsigned long frameDelay, unsigned long durationMs)
{
    randomMotion(motionStrength, getColorFromString(colorName), frameDelay, durationMs);
}

// Random motion animation that simulates dots moving across the LED strip
// motionStrength: -1.0 = 100% left motion, +1.0 = 100% right motion, 0.0 = random direction
// color: Color of the moving dots (default: blue)
// frameDelay: Delay between frames in milliseconds (default: 75)
// durationMs: Duration of the animation in milliseconds (default: 2000)
void FED4::randomMotion(float motionStrength, uint32_t color, unsigned long frameDelay, unsigned long durationMs)
{
    // Clamp motionStrength to valid range
    if (motionStrength < -1.0f) motionStrength = -1.0f;
    if (motionStrength > 1.0f) motionStrength = 1.0f;
    
    // Convert motionStrength to probability of rightward motion
    // -1.0 -> 0.0 (0% right, 100% left)
    //  0.0 -> 0.5 (50% right, 50% left)
    // +1.0 -> 1.0 (100% right, 0% left)
    float probRight = (motionStrength + 1.0f) / 2.0f;
    
    // Structure to track moving dots
    struct Dot {
        int position;      // Current position (1 to 6)
        bool active;        // Whether this dot is active
    };
    
    // Maximum number of dots active at once
    const int MAX_DOTS = 5;
    Dot dots[MAX_DOTS];
    
    // Initialize all dots as inactive
    for (int i = 0; i < MAX_DOTS; i++) {
        dots[i].active = false;
    }
    
    // Track completed cycles (dots that have moved off the visible area)
    unsigned int completedCycles = 0;
    const unsigned int MAX_CYCLES = 3;
    
    // Run animation for the specified duration
    unsigned long startTime = millis();
    while (completedCycles < MAX_CYCLES && (millis() - startTime) < durationMs) {
        // Clear the strip
        fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
        
        // Update existing dots - direction determined probabilistically each step
        for (int i = 0; i < MAX_DOTS; i++) {
            if (dots[i].active) {
                // Determine movement direction probabilistically based on motionStrength
                float randVal = (float)random(0, 10000) / 10000.0f;
                int moveDirection;
                
                if (randVal < probRight) {
                    // Move right
                    moveDirection = 1;
                } else {
                    // Move left
                    moveDirection = -1;
                }
                
                // Move the dot
                dots[i].position += moveDirection;
                
                // Check if dot has moved off the visible area (positions 0 and 7 are excluded)
                // Dots are only visible in positions 1 to 6
                if (dots[i].position < 1 || dots[i].position > 6) {
                    // Dot moved off the visible area
                    completedCycles++;
                    dots[i].active = false;
                } else {
                    // Draw the dot at its current position (only positions 1 to 6)
                    strip_leds[dots[i].position] = color;
                }
            }
        }
        
        // Randomly spawn new dots anywhere in the visible range
        // Ensure minimum of 2 dots are always active
        int activeCount = 0;
        for (int i = 0; i < MAX_DOTS; i++) {
            if (dots[i].active) activeCount++;
        }
        
        const int MIN_DOTS = 2;
        
        // If we have fewer than minimum dots, always spawn (100% chance)
        // Otherwise, spawn with probability based on active count
        bool shouldSpawn = false;
        if (activeCount < MIN_DOTS) {
            shouldSpawn = true; // Always spawn if below minimum
        } else if (activeCount < MAX_DOTS && random(0, 100) < 30) {
            shouldSpawn = true; // 30% chance per frame when not at max
        }
        
        if (shouldSpawn) {
            // Find an inactive dot slot
            for (int i = 0; i < MAX_DOTS; i++) {
                if (!dots[i].active) {
                    // Spawn dot at random position in visible range (1 to 6)
                    dots[i].position = random(1, 7); // random between 1 and 6 inclusive
                    dots[i].active = true;
                    // Draw the newly spawned dot immediately at its spawn position
                    strip_leds[dots[i].position] = color;
                    break;
                }
            }
        }
        
        // Show the frame
        FastLED.show();
        delay(frameDelay);
        
        // Stop if we've completed 3 cycles
        if (completedCycles >= MAX_CYCLES) {
            break;
        }
    }
    
    // Clear the strip at the end
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    FastLED.show();
}

// Clear the strip
// Example usage:
// lightsOff();    // Clears the front LEDs on FED4
void FED4::lightsOff()
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
    delay(1);
}

// Example usage:
// Light up left port with specific colors and brightness:
// leftLight("red", 100);    // Set left port to red with brightness 100
// leftLight("green", 50);   // Set left port to green with brightness 50
// leftLight("blue", 200);   // Set left port to blue with brightness 200
// leftLight("yellow", 1);   // Set left port to yellow with minimum brightness 10
void FED4::leftLight(uint32_t color, uint8_t brightness)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    // Enforce minimum brightness of 10
    if (brightness < 10) {
        brightness = 10;
    }
    // Apply brightness scaling to the color
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;
    uint32_t dimmedColor = (r << 16) | (g << 8) | b;
    strip_leds[5] = dimmedColor;
    strip_leds[6] = dimmedColor;
    strip_leds[7] = dimmedColor;
    FastLED.show();
    delay(1);

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
    delay(1);

}

// Example usage:
// Light up center port with specific colors and brightness:
// centerLight("red", 100);    // Set center port to red with brightness 100
// centerLight("green", 50);   // Set center port to green with brightness 50
// centerLight("blue", 200);   // Set center port to blue with brightness 200
// centerLight("yellow", 1);   // Set center port to yellow with minimum brightness 10
void FED4::centerLight(uint32_t color, uint8_t brightness)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    // Enforce minimum brightness of 10
    if (brightness < 10) {
        brightness = 10;
    }
    // Apply brightness scaling to the color
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;
    uint32_t dimmedColor = (r << 16) | (g << 8) | b;
    strip_leds[3] = dimmedColor;
    strip_leds[4] = dimmedColor;
    FastLED.show();
    delay(1);
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
    delay(1);
}

// Example usage:
// Light up right port with specific colors and brightness:
// rightLight("red", 100);    // Set right port to red with brightness 100
// rightLight("green", 50);   // Set right port to green with brightness 50
// rightLight("blue", 200);   // Set right port to blue with brightness 200
// rightLight("yellow", 1);   // Set right port to yellow with minimum brightness 10
void FED4::rightLight(uint32_t color, uint8_t brightness)
{
    fill_solid(strip_leds, NUM_STRIP_LEDS, CRGB::Black);
    // Enforce minimum brightness of 20
    if (brightness < 20) {
        brightness = 20;
    }
    // Apply brightness scaling to the color
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;
    uint32_t dimmedColor = (r << 16) | (g << 8) | b;
    strip_leds[0] = dimmedColor;
    strip_leds[1] = dimmedColor;
    strip_leds[2] = dimmedColor;
    FastLED.show();
    delay(1);
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
// Light up left port using color names with brightness:
// leftLight("red", 100);     // Set left port to red with brightness 100
// leftLight("green", 50);    // Set left port to green with brightness 50
// leftLight("blue", 200);    // Set left port to blue with brightness 200
// leftLight("yellow", 1);    // Set left port to yellow with minimum brightness 10
void FED4::leftLight(const char *colorName, uint8_t brightness)
{
    // Enforce minimum brightness of 10
    if (brightness < 10) {
        brightness = 10;
    }
    leftLight(getColorFromString(colorName), brightness);
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
// Light up center port using color names with brightness:
// centerLight("red", 100);     // Set center port to red with brightness 100
// centerLight("green", 50);    // Set center port to green with brightness 50
// centerLight("blue", 200);    // Set center port to blue with brightness 200
// centerLight("yellow", 1);    // Set center port to yellow with minimum brightness 10
void FED4::centerLight(const char *colorName, uint8_t brightness)
{
    // Enforce minimum brightness of 10
    if (brightness < 10) {
        brightness = 10;
    }
    centerLight(getColorFromString(colorName), brightness);
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
// Light up right port using color names with brightness:
// rightLight("red", 100);     // Set right port to red with brightness 100
// rightLight("green", 50);    // Set right port to green with brightness 50
// rightLight("blue", 200);    // Set right port to blue with brightness 200
// rightLight("yellow", 1);    // Set right port to yellow with minimum brightness 10
void FED4::rightLight(const char *colorName, uint8_t brightness)
{
    // Enforce minimum brightness of 10
    if (brightness < 10) {
        brightness = 10;
    }
    rightLight(getColorFromString(colorName), brightness);
}

// ── STATUS LED ──────────────────────────────────────────────────────────────
// Single red LED on GPIO STATUS_LED — digital on/off only (no analogWrite).
// ESP32-S3 LEDC timers share one clock with VCOM KEEP_ALIVE; PWM here would
// fight DISPLAY_VCOM LEDC (see FED4-VCOM-LEDC-Light-Sleep).
// API: redPix(brightness) lights if brightness > 0; noPix() turns off.

bool FED4::initializePixel()
{
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    statusLedBrightness = 0;
    return true;
}

// Lights the red LED when brightness > 0 (digital; brightness value not PWM).
void FED4::redPix(uint8_t brightness)
{
    statusLedBrightness = brightness;
    digitalWrite(STATUS_LED, brightness > 0 ? HIGH : LOW);
}

// Turns the status LED off.
void FED4::noPix()
{
    statusLedBrightness = 0;
    digitalWrite(STATUS_LED, LOW);
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