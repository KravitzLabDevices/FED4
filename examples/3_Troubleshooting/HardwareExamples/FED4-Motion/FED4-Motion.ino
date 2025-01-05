#include "SparkFun_STHS34PF80_Arduino_Library.h"
#include <Wire.h>


STHS34PF80_I2C mySensor2;

int16_t presenceVal2 = 0;
int16_t motionVal2 = 0;
float temperatureVal2 = 0;

uint16_t threshold = 1000;
uint8_t hysteresis = 100;

int timer = 0;
// Create a second TwoWire instance
TwoWire I2C_2 = TwoWire(1);

void setup()
{
	Serial.begin(115200);
	Serial.println("STHS34PF80 Example: Multiple Sensors");
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);

	delay(1000);
	if (I2C_2.begin(20, 19) == 0)
	{
		Serial.println("I2C_2 Error - check I2C Address");
		while (1)
			;
	}
	delay(1000);

	// Establish communication with second device
	while (mySensor2.begin(0x5A, I2C_2) == false)
	{
    Serial.println("No Sensor Found");
		timer++;
		delay(1000);
		Serial.println(timer);
	}

	mySensor2.setPresenceThreshold(threshold);
	mySensor2.setPresenceHysteresis(hysteresis);

	delay(1000);
}

void loop()
{
	sths34pf80_tmos_drdy_status_t dataReady2;

	mySensor2.getDataReady(&dataReady2);

	// Check whether second sensor has new data - run through loop if data is ready
	if (dataReady2.drdy == 1)
	{
		sths34pf80_tmos_func_status_t status2;
		mySensor2.getStatus(&status2);

		if (status2.pres_flag == 1)
		{
			mySensor2.getPresenceValue(&presenceVal2);
			Serial.println("Presence ");
		}
		else
		{
			presenceVal2 = 0; // Reset presence value if no presence detected
			Serial.println("");
		}
	}

	delay(100); // Add a slight delay to stabilize the readings
}
