#include "FED4.h"

void FED4::serialStatusReport()
{
    if (wakeCount == 0) {
        Serial.print("Ready to go!");
    }
    Serial.printf(", WakeCount=%d ||| %02d/%02d/%02d %02d:%02d:%02d | Temp=%.1fC | Hum=%.1f%% | Bat=%.1fV(%.1f%%) | Pellets=%d | Left/Center/Right=%d/%d/%d \n",
        wakeCount,
        rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
        temperature, humidity, 
        cellVoltage, cellPercent,
        pelletCount,
        leftCount, centerCount, rightCount
    );
}