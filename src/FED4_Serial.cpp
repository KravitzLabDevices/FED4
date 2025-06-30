#include "FED4.h"

void FED4::serialStatusReport()
{
    if (wakeCount == 0) {
        Serial.print("Ready to go!");
    }
    Serial.printf(" - Wake %d | %02d/%02d/%02d %02d:%02d:%02d | Temp %.1fC | Hum %.1f%% | Light %.1f | Bat %.1fV(%.1f%%) | Motion %d | Pellets %d | Left/Center/Right %d/%d/%d | FreeHeap %d\n",        
        wakeCount,
        rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
        temperature, humidity, lux,
        cellVoltage, cellPercent,
        motionDetected ? "   Motion" : "No Motion",
        pelletCount,    
        leftCount, centerCount, rightCount,
        ESP.getFreeHeap());
}