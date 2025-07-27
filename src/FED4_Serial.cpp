#include "FED4.h"

void FED4::serialStatusReport()
{
    if (wakeCount == 0) {
        Serial.print("Ready to go!");
    }
    Serial.printf(" - Wake %d | %02d/%02d/%02d %02d:%02d:%02d | %.1fC | RH %.1f%% | Lux %.3f (%.3f) | Bat %.1fV(%.1f%%) | Motion %d(%.1f%%) | Pellets %d | Left/Center/Right %d/%d/%d | FreeMem %d\n",        
        wakeCount,
        rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
        temperature, humidity, lux, white,
        cellVoltage, cellPercent,
        motionDetected, motionPercentage,
        pelletCount,    
        leftCount, centerCount, rightCount,
        ESP.getFreeHeap());
}