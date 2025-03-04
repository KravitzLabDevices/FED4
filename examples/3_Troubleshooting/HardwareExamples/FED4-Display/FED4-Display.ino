/*
Basic code to display text on external display.

Main thing needed are the GPIO expander library, then turn on the 3rd LDO via the expander which is what powers the display.

*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MCP23X17.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET -1	  // can set an oled reset pin if desired
#define SDA_2 20
#define SCL_2 19
#define LDO3 14

Adafruit_MCP23X17 mcp;
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);

void setup()
{
	// Initialize serial communication for debugging purposes
	Serial.begin(115200);

	if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1)
			;
	}

	mcp.pinMode(LDO3, OUTPUT);
	mcp.digitalWrite(LDO3, HIGH);

	if (!display.begin(0x3D, true))
	{ // Address 0x3D default for SH1107
		Serial.println("SSD1306 allocation failed");
		for (;;)
			;
	}

	// Clear the display buffer
	display.clearDisplay();

	// Set text size and color
	display.setTextSize(4);				// Normal 1:1 pixel scale
	display.setTextColor(SH110X_WHITE); // Draw white text

	// Set cursor to top-left corner
	display.setCursor(19, 60);

	// Display static text
	display.println("FED4");

	// Update the display with the new text
	display.display();
}

void loop()
{
	// Nothing to do here
}
