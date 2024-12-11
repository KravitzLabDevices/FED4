#include "Adafruit_MLX90393.h"
#include <Wire.h>

TwoWire I2C_2 = TwoWire(1);
#define SDA_2 20
#define SCL_2 19
#define MLX90393_CS 10

Adafruit_MLX90393 sensor = Adafruit_MLX90393();

void setup(void)
{
	Serial.begin(115200);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
	I2C_2.begin(SDA_2, SCL_2);
	// delay(100); // Small delay to stabilize I2C

	Serial.println("Starting Adafruit MLX90393 Demo");
	delay(100);
	if (!sensor.begin_I2C(0x0C, &I2C_2))
	{
		Serial.println("No sensor found ... check your wiring?");
		while (1)
		{
			delay(10);
		}
	}
	Serial.println("Found a MLX90393 sensor");

	sensor.setGain(MLX90393_GAIN_5X);
	Serial.print("Gain set to: ");
	switch (sensor.getGain())
	{
	case MLX90393_GAIN_1X:
		Serial.println("1 x");
		break;
	case MLX90393_GAIN_1_33X:
		Serial.println("1.33 x");
		break;
	case MLX90393_GAIN_1_67X:
		Serial.println("1.67 x");
		break;
	case MLX90393_GAIN_2X:
		Serial.println("2 x");
		break;
	case MLX90393_GAIN_2_5X:
		Serial.println("2.5 x");
		break;
	case MLX90393_GAIN_3X:
		Serial.println("3 x");
		break;
	case MLX90393_GAIN_4X:
		Serial.println("4 x");
		break;
	case MLX90393_GAIN_5X:
		Serial.println("5 x");
		break;
	}

	sensor.setResolution(MLX90393_X, MLX90393_RES_17);
	sensor.setResolution(MLX90393_Y, MLX90393_RES_17);
	sensor.setResolution(MLX90393_Z, MLX90393_RES_16);
	sensor.setOversampling(MLX90393_OSR_3);
	sensor.setFilter(MLX90393_FILTER_5);
}

void loop(void)
{
	float x, y, z;

	if (sensor.readData(&x, &y, &z))
	{
		Serial.print("X: ");
		Serial.print(x, 4);
		Serial.println(" uT");
		Serial.print("Y: ");
		Serial.print(y, 4);
		Serial.println(" uT");
		Serial.print("Z: ");
		Serial.print(z, 4);
		Serial.println(" uT");
	}
	else
	{
		Serial.println("Failed to read data");
	}

	delay(500);

	sensors_event_t event;
	sensor.getEvent(&event);
	Serial.print("X: ");
	Serial.print(event.magnetic.x);
	Serial.print(" \tY: ");
	Serial.print(event.magnetic.y);
	Serial.print(" \tZ: ");
	Serial.print(event.magnetic.z);
	Serial.println(" uTesla ");

	delay(500);
}
