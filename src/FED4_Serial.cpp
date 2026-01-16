#include "FED4.h"

void FED4::serialStatusReport()
{
    if (wakeCount == 0) {
        Serial.println("********** Ready to go! **********");
    }
    
    // Check if motion sensor is disabled
    if (!useMotionSensor || isnan(motionPercentage)) {
        Serial.printf("%02d/%02d/%02d %02d:%02d:%02d | %.1fC - %.1f%% - %.1fhPa - %.1fKΩ | %.1fLux(%.0f) | Bat %.2fV(%.1f%%) | Motion disabled | Pellets %d | Left/Center/Right %d/%d/%d | FreeMem %d | Wake %d\n",        
            rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
            temperature, humidity, pressure, gasResistance, lux, white,
            cellVoltage, cellPercent,
            pelletCount,    
            leftCount, centerCount, rightCount,
            ESP.getFreeHeap(),
            wakeCount);
    } else {
        Serial.printf("%02d/%02d/%02d %02d:%02d:%02d | %.1fC - %.1f%% - %.1fhPa - %.1fKΩ | %.1fLux(%.0f) | Bat %.2fV(%.1f%%) | Motion %d(%.1f%%) | Pellets %d | Left/Center/Right %d/%d/%d | FreeMem %d | Wake %d\n",        
            rtc.now().month(), rtc.now().day(), rtc.now().year(), rtc.now().hour(), rtc.now().minute(), rtc.now().second(),
            temperature, humidity, pressure, gasResistance, lux, white,
            cellVoltage, cellPercent,
            motionDetected, motionPercentage,
            pelletCount,    
            leftCount, centerCount, rightCount,
            ESP.getFreeHeap(),
            wakeCount);
    }
}