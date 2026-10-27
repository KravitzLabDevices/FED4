#include "FED4.h"

void FED4::serialStatusReport()
{
    if (wakeCount == 0) {
        Serial.println("********** Ready to go! **********");
    }

    char motionText[40];
    if (!useMotionSensor) {
        snprintf(motionText, sizeof(motionText), "No Motion");
    } else if (!pirCounterAvailable) {
        snprintf(motionText, sizeof(motionText), "No Counter");
    } else if (activityWindowSeconds >= 60) {
        snprintf(motionText, sizeof(motionText), "Activity %.1f%% (%lum)",
                 activityPercent, (unsigned long)(activityWindowSeconds / 60));
    } else {
        snprintf(motionText, sizeof(motionText), "Activity %.1f%% (%lus)",
                 activityPercent, (unsigned long)activityWindowSeconds);
    }

    Serial.printf("%02d/%02d/%02d %02d:%02d:%02d | %.1fC - %.1f%% - %.1fhPa - %.1fKΩ | %.1fLux | %.2fV(%.1f%%) | %s | Pel %d | Left/Cent/Right %d/%d/%d | Poke %.0fms | Mem %d | Wake %d\n",
        rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
        temperature, humidity, pressure, gasResistance, lux,
        cellVoltage, cellPercent,
        motionText,
        pelletCount,
        leftCount, centerCount, rightCount,
        pokeDuration,
        ESP.getFreeHeap(),
        wakeCount);
}
