#include <Adafruit_AHTX0.h>
#include <Wire.h>
#define SDA_2 20
#define SCL_2 19

TwoWire I2C_2 = TwoWire(1);
Adafruit_AHTX0 aht;

void setup()
{
	Serial.begin(115200);
	while (!Serial)
		; // Wait for Serial to be ready
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
	Wire.begin(); // wait until serial monitor opens
	I2C_2.begin(SDA_2, SCL_2);

	if (!aht.begin(&I2C_2))
	{
		Serial.println("Could not find AHT? Check wiring");
		while (1)
			delay(10);
	}
	Serial.println("AHT10 or AHT20 found");
}

void loop()
{
	sensors_event_t humidity, temp;
	aht.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data
	Serial.print("Temperature: ");
	Serial.print(temp.temperature);
	Serial.println(" degrees C");
	Serial.print("Humidity: ");
	Serial.print(humidity.relative_humidity);
	Serial.println("% rH");

	delay(500);
}