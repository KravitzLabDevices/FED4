#include "FED4.h"

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
    Serial.print(temperature, 1);
    Serial.println(F(" °C"));
    Serial.print(F("  • Humidity:     "));
    Serial.print(humidity, 1);
    Serial.println(F("%"));

    // Battery Status section
    Serial.println(F("\n► Battery Status"));
    Serial.print(F("  • Voltage: "));
    Serial.print(cellVoltage, 1);
    Serial.println(F("V"));
    Serial.print(F("  • Charge:  "));
    Serial.print(cellPercent, 1);
    Serial.println(F("%"));

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

    Serial.println();
    Serial.print("Time since last sensor poll: ");
    Serial.print((millis()-lastPollTime)/1000);
    Serial.print("s");

    Serial.println(F("\n================================================\n"));
}
