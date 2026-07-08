/*
 * FED4 RTC Test
 *
 * DS3231 RTC is on main I2C but behind the TCA4307 isolator on PSV2 rail.
 * Enable PSV2 (MCP pin 13) before rtc.begin().
 */

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "RTClib.h"

#define SDA_PIN 8
#define SCL_PIN 9
#define EXP_PSV2_EN 13

RTC_DS3231 rtc;
Adafruit_MCP23X17 mcp;

const char *daysOfTheWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed.");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
  delay(5);

  if (!rtc.begin(&Wire)) {
    Serial.println("Couldn't find RTC.");
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  DateTime now = rtc.now();
  Serial.printf("%04d/%02d/%02d (%s) %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                daysOfTheWeek[now.dayOfTheWeek()],
                now.hour(), now.minute(), now.second());
  Serial.printf("Unix: %lu  Temp: %.2f C\n\n",
                (unsigned long)now.unixtime(),
                rtc.getTemperature());
  delay(1000);
}
