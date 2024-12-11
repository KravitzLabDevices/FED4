// Date and time functions using a DS3231 RTC connected via I2C_2 and Wire lib
#include "RTClib.h"
#include <Wire.h>
RTC_DS3231 rtc;

TwoWire I2C_2 = TwoWire(1);
#define SDA_2 20
#define SCL_2 19

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup()
{
	Serial.begin(115200);
	while (!Serial)
		; // Wait for Serial to be ready
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);

	Wire.begin(); // wait until serial monitor opens
	I2C_2.begin(SDA_2, SCL_2);

	if (!rtc.begin(&I2C_2))
	{
		Serial.println("Couldn't find RTC");
		Serial.flush();
		while (1)
			delay(10);
	}

	if (rtc.lostPower())
	{
		Serial.println("RTC lost power, let's set the time!");
		// When time needs to be set on a new device, or after a power loss, the
		// following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
	}

	// When time needs to be re-set on a previously configured device, the
	// following line sets the RTC to the date & time this sketch was compiled
	// rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
	// This line sets the RTC with an explicit date & time, for example to set
	// January 21, 2014 at 3am you would call:
	// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
}

void loop()
{
	DateTime now = rtc.now();

	Serial.print(now.year(), DEC);
	Serial.print('/');
	Serial.print(now.month(), DEC);
	Serial.print('/');
	Serial.print(now.day(), DEC);
	Serial.print(" (");
	Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
	Serial.print(") ");
	Serial.print(now.hour(), DEC);
	Serial.print(':');
	Serial.print(now.minute(), DEC);
	Serial.print(':');
	Serial.print(now.second(), DEC);
	Serial.println();

	Serial.print(" since midnight 1/1/1970 = ");
	Serial.print(now.unixtime());
	Serial.print("s = ");
	Serial.print(now.unixtime() / 86400L);
	Serial.println("d");

	// calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
	DateTime future(now + TimeSpan(7, 12, 30, 6));

	Serial.print(" now + 7d + 12h + 30m + 6s: ");
	Serial.print(future.year(), DEC);
	Serial.print('/');
	Serial.print(future.month(), DEC);
	Serial.print('/');
	Serial.print(future.day(), DEC);
	Serial.print(' ');
	Serial.print(future.hour(), DEC);
	Serial.print(':');
	Serial.print(future.minute(), DEC);
	Serial.print(':');
	Serial.print(future.second(), DEC);
	Serial.println();

	Serial.print("Temperature: ");
	Serial.print(rtc.getTemperature());
	Serial.println(" C");

	Serial.println();
	delay(3000);
}
