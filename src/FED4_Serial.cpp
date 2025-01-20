#include "FED4.h"

void FED4::serialStatusReport()
{
    Serial.printf("FED4 Status: %02d/%02d/%02d %02d:%02d:%02d | %.1fC | %.1fV(%.1f%%) | Pellets=%d | Left/Center/Right=%d/%d/%d\n",
        rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
        temperature, 
        cellVoltage, cellPercent,
        pelletCount,
        leftCount, centerCount, rightCount
    );
}