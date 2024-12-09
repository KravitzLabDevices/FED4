#include "FED4.h"

void FED4::updateDisplay()
{
    // Initialize display
    Serial.println("Initializing display...");
    display.begin();
    display.refresh();
    display.setRotation(2);
    display.setTextColor(BLACK);
    display.clearDisplay();
    Serial.println("Display initialized");

    // Display title
    Serial.println("Drawing title...");
    display.setTextSize(3);
    display.setCursor(12, 20);
    display.print("FED4");
    display.refresh();
    Serial.println("Title drawn");

    // Switch to smaller text for details
    display.setTextSize(1);

    // Display counts
    Serial.println("Drawing counts...");
    display.setCursor(12, 56);
    display.print("Pellets: ");
    display.print(pelletCount);
    display.setCursor(12, 72);
    display.print("L:");
    display.print(leftCount);
    display.print("   C:");
    display.print(centerCount);
    display.print("   R:");
    display.print(rightCount);
    Serial.print("Counts - Pellets: ");
    Serial.print(pelletCount);
    Serial.print(" Left: ");
    Serial.print(leftCount);
    Serial.print(" Center: ");
    Serial.print(centerCount);
    Serial.print(" Right: ");
    Serial.println(rightCount);

    // Display environmental data
    Serial.println("Getting environmental data...");
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    display.setCursor(12, 90);
    display.print("Temp: ");
    display.print(temp.temperature, 1);
    display.print("C");
    display.print(" Hum: ");
    display.print(humidity.relative_humidity, 1);
    display.println("%");
    Serial.print("Temperature: ");
    Serial.print(temp.temperature, 1);
    Serial.print("C, Humidity: ");
    Serial.print(humidity.relative_humidity, 1);
    Serial.println("%");

    // Display battery status
    Serial.println("Getting battery status...");
    float cellVoltage = maxlipo.cellVoltage();
    float cellPercent = maxlipo.cellPercent();
    display.setCursor(12, 122);
    display.print("Fuel: ");
    display.print(cellVoltage, 1);
    display.print("V, ");
    display.print(cellPercent, 1);
    display.println("%");
    Serial.print("Battery: ");
    Serial.print(cellVoltage, 1);
    Serial.print("V, ");
    Serial.print(cellPercent, 1);
    Serial.println("%");

    // Display date/time
    Serial.println("Getting date/time...");
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
    Serial.print("DateTime: ");
    Serial.print(now.month());
    Serial.print("/");
    Serial.print(now.day());
    Serial.print("/");
    Serial.print(now.year());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(":");
    if (now.minute() < 10)
        Serial.print('0');
    Serial.println(now.minute());

    // Display wake count
    Serial.println("Drawing wake count...");
    display.setCursor(12, 156);
    display.print("Unclear:");
    display.print(wakeCount);
    Serial.print("Wake count: ");
    Serial.println(wakeCount);

    display.refresh();
    Serial.println("Display refresh complete");

    serialStatusReport();
}

void FED4::serialStatusReport()
{
    // Print header
    Serial.println(F("\n================================================"));
    Serial.println(F("                 FED4 Status Report               "));
    Serial.println(F("================================================"));

    // Date and Time section
    Serial.print(F("\n► DateTime: "));
    serialPrintRTC();
    Serial.println();

    // Environmental Data section
    Serial.println(F("\n► Environmental Data"));
    Serial.print(F("  • Temperature:  "));
    Serial.print(getTemperature(), 1);
    Serial.println(F(" °C"));
    Serial.print(F("  • Humidity:     "));
    Serial.print(getHumidity(), 1);
    Serial.println(F("%"));

    // Battery Status section
    Serial.println(F("\n► Battery Status"));
    if (isBatteryConnected())
    {
        Serial.print(F("  • Voltage: "));
        Serial.print(getBatteryVoltage(), 1);
        Serial.println(F("V"));
        Serial.print(F("  • Charge:  "));
        Serial.print(getBatteryPercentage(), 1);
        Serial.println(F("%"));
    }
    else
    {
        Serial.println(F("  • USB Powered"));
    }

    // Pellet counts section
    Serial.println(F("\n► Pellet Count"));
    Serial.print(F("  • Total: "));
    Serial.println(pelletCount);

    Serial.println(F("\n► Pokes"));
    Serial.print(F("  • Left: "));
    Serial.print(leftCount);
    Serial.print(F(" | Center: "));
    Serial.print(centerCount);
    Serial.print(F(" | Right: "));
    Serial.println(rightCount);

    // Memory Statistics section
    Serial.println(F("\n► Memory Statistics"));
    Serial.print(F("  • Free Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    Serial.print(F("  • Heap Size: "));
    Serial.print(ESP.getHeapSize());
    Serial.println(F(" bytes"));
    Serial.print(F("  • Min Free Heap: "));
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(F(" bytes"));

    Serial.println(F("\n================================================\n"));
}