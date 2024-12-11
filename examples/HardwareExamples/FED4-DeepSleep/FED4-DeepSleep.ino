#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23X17.h>
#include "SparkFun_STHS34PF80_Arduino_Library.h"

#define PG1 12
#define PG2 13
#define PG3 0
#define PG4 11
#define PIN 35
#define NUMPIXELS 1
#define intPin 16

Adafruit_MCP23X17 mcp;
STHS34PF80_I2C mySensor1;
STHS34PF80_I2C mySensor2;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Create a second TwoWire instance
TwoWire I2C_2 = TwoWire(1);


uint16_t presenceThreshold = 500; // you can adjust these 2 to affect the senstivity of the presence detection.
uint8_t presenceHysteresis = 50;  //
int DELAYVAL = 2000;
int bootCount = 0;
int timer = 0;
bool volatile interruptFlag = false;

// ISR to set the triggered interrupt
void isr1()
{
	interruptFlag = true;
}

void print_wakeup_reason()
{
	esp_sleep_wakeup_cause_t wakeup_reason;

	wakeup_reason = esp_sleep_get_wakeup_cause();

	switch (wakeup_reason)
	{
	case ESP_SLEEP_WAKEUP_EXT0:
		Serial.println("Wakeup caused by external signal using RTC_IO");
		break;
	case ESP_SLEEP_WAKEUP_EXT1:
		Serial.println("Wakeup caused by external signal using RTC_CNTL");
		break;
	case ESP_SLEEP_WAKEUP_TIMER:
		Serial.println("Wakeup caused by timer");
		break;
	case ESP_SLEEP_WAKEUP_TOUCHPAD:
		Serial.println("Wakeup caused by touchpad");
		break;
	case ESP_SLEEP_WAKEUP_ULP:
		Serial.println("Wakeup caused by ULP program");
		break;
	default:
		Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
		break;
	}
}

void setup()
{
	Serial.begin(115200);

	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);

	delay(30);

	if (Wire.begin() == 0)
	{
		Serial.println("I2C Error - check I2C Address");
		while (1)
			;
	}

	Serial.println("Starting...");
	if (!mcp.begin_I2C())
	{
		Serial.println("Error.");
		while (1)
			;
	}

	// Begin I2C on the second bus
	if (I2C_2.begin(20, 19) == 0)
	{
		Serial.println("I2C_2 Error - check I2C Address");
		while (1)
			;
	}

	mcp.pinMode(PG1, INPUT_PULLUP);
	mcp.pinMode(PG2, INPUT_PULLUP);
	mcp.pinMode(PG3, INPUT_PULLUP);

	if (mySensor1.begin() == false)
	{
		Serial.println("Error setting up device 1 - please check wiring.");
		while (1)
			;
	}

	while (mySensor2.begin(0x5A, I2C_2) == false)
	{
		timer++;
		delay(100);
		Serial.println(timer);
	}

	mySensor1.setPresenceThreshold(presenceThreshold);
	mySensor1.setPresenceHysteresis(presenceHysteresis);
	mySensor1.setTmosODR(STHS34PF80_TMOS_ODR_AT_15Hz); // change this to effect the rate at which the sensor is looking for the presence of a mouse. the faster it is the more responsive it is.

	mySensor2.setPresenceThreshold(presenceThreshold);
	mySensor2.setPresenceHysteresis(presenceHysteresis);
	mySensor2.setTmosODR(STHS34PF80_TMOS_ODR_AT_15Hz); // change this to effect the rate at which the sensor is looking for the presence of a mouse. the faster it is the more responsive it is.

	/*
	STHS34PF80_TMOS_ODR_AT_0Hz25 = 135uA (VERY SLOW)
	STHS34PF80_TMOS_ODR_AT_2Hz = 150uA (SLOW)
	STHS34PF80_TMOS_ODR_AT_8Hz = 200uA (AVERAGE)
	STHS34PF80_TMOS_ODR_AT_15Hz = 270uA (FAST)
	STHS34PF80_TMOS_ODR_AT_30Hz = 400uA (FASTEST...PROBABLY OVERKILL)

	8 or 15 seems to work the best, but play around with it, 4 might work just fine for mice who knows.
	*/

	attachInterrupt(digitalPinToInterrupt(GPIO_NUM_16), isr1, CHANGE);

	// Route all interrupts from device to interrupt pin
	mySensor1.setTmosRouteInterrupt(STHS34PF80_TMOS_INT_OR);
	mySensor1.setTmosInterruptOR(STHS34PF80_TMOS_INT_PRESENCE);
	mySensor1.setInterruptPulsed(0);

	mySensor2.setTmosRouteInterrupt(STHS34PF80_TMOS_INT_OR);
	mySensor2.setTmosInterruptOR(STHS34PF80_TMOS_INT_PRESENCE);
	mySensor2.setInterruptPulsed(0);

	// Increment boot number and print it every reboot
	++bootCount;
	Serial.println("Boot number: " + String(bootCount));

	// Print the wakeup reason for ESP32
	print_wakeup_reason();

	// Clear the interrupt flag at the beginning
	interruptFlag = false;

	pixels.clear();
	for (int i = 0; i < NUMPIXELS; i++)
	{
		pixels.setPixelColor(i, pixels.Color(150, 150, 150));
		pixels.show();
		delay(DELAYVAL);
	}

	// Enable wakeup on external pin
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_16, 1);
}

void loop()
{
	int PG1Read = mcp.digitalRead(PG1);
	int PG2Read = mcp.digitalRead(PG2);
	int PG3Read = mcp.digitalRead(PG3);

	if (PG1Read == LOW)
	{
		Serial.println("Center Nose Poke");
		pixels.clear();

		pixels.setPixelColor(0, pixels.Color(150, 0, 0));
		pixels.show();

		Serial.println("Going to sleep in 2 seconds...");
		delay(2000);
		delay(10);
		esp_deep_sleep_start();
	}
	if (PG2Read == LOW)
	{
		Serial.println("Right Nose Poke");
		pixels.clear();

		pixels.setPixelColor(0, pixels.Color(0, 150, 0));
		pixels.show();

		Serial.println("Going to sleep in 2 seconds...");
		delay(2000);
		delay(10);
		esp_deep_sleep_start();
	}
	if (PG3Read == LOW)
	{
		Serial.println("Left Nose Poke");
		pixels.clear();

		pixels.setPixelColor(0, pixels.Color(0, 0, 150));
		pixels.show();

		Serial.println("Going to sleep in 2 seconds...");
		delay(2000);
		delay(10);
		esp_deep_sleep_start();
	}
}