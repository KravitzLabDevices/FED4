#include "FED4.h"

void FED4::UpdateDisplay()
{
    SerialOutput();

    // Initialize display
    display.begin();
    display.refresh();
    display.setRotation(2);
    display.setTextColor(BLACK);
    display.clearDisplay();

    // Display title
    display.setTextSize(3);
    display.setCursor(12, 20);
    display.print("FED4");
    display.refresh();

    // Switch to smaller text for details
    display.setTextSize(1);

    // Display counts
    display.setCursor(12, 56);
    display.print("Pellets: ");
    display.print(PelletCount);
    display.setCursor(12, 72);
    display.print("L:");
    display.print(LeftCount);
    display.print("   C:");
    display.print(CenterCount);
    display.print("   R:");
    display.print(RightCount);

    // Display environmental data
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    display.setCursor(12, 90);
    display.print("Temp: ");
    display.print(temp.temperature, 1);
    display.print("C");
    display.print(" Hum: ");
    display.print(humidity.relative_humidity, 1);
    display.println("%");

    // Display battery status
    float cellVoltage = maxlipo.cellVoltage();
    float cellPercent = maxlipo.cellPercent();
    display.setCursor(12, 122);
    display.print("Fuel: ");
    display.print(cellVoltage, 1);
    display.print("V, ");
    display.print(cellPercent, 1);
    display.println("%");

    // Display date/time
    DateTime now = rtc.now();
    display.setCursor(12, 140);
    display.print(now.month());
    display.print("/");
    display.print(now.day());
    display.print("/");
    display.print(now.year());
    display.print(" ");
    display.print(now.hour());
    display.print(":");
    if (now.minute() < 10)
        display.print('0');
    display.print(now.minute());

    // Display wake count
    display.setCursor(12, 156);
    display.print("Unclear:");
    display.print(WakeCount);

    display.refresh();
}

void FED4::SerialOutput()
{
    DateTime now = rtc.now();

    // Output date and time
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print("    ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    // Output temperature
    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");
    Serial.println();

    // Output counts
    Serial.print("Pellets: ");
    Serial.print(PelletCount);
    Serial.print(" Center: ");
    Serial.print(CenterCount);
    Serial.print(" Left: ");
    Serial.print(LeftCount);
    Serial.print("  Right: ");
    Serial.println(RightCount);
}