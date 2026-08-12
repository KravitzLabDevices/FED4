#include "FED4.h"

// Motion detection using EKMB1107112 PIR sensor (digital push-pull output on PIR_MOTION pin).
// The PIR pin is configured in begin() alongside other GPIO pins.
// STATUS_LED mirrors PIR (same as FED4-Demo-Hardware / FED4-PIR-Sensor).

void FED4::updateStatusLedFromMotion()
{
    if (!useMotionSensor) {
        return;
    }
    analogWrite(STATUS_LED, digitalRead(PIR_MOTION) ? 255 : 0);
}

bool FED4::motion()
{
    if (!useMotionSensor) {
        return false;
    }

    bool motionFlag = digitalRead(PIR_MOTION);
    updateStatusLedFromMotion();

    Serial.print("PIR: ");
    Serial.print(motionFlag ? "MOTION" : "clear");

    if (motionFlag) {
        motionCount++;
        motionPercentage = (pollCount > 0) ? (float)motionCount / pollCount * 100.0 : 0.0;

        Serial.print(" - MOTION DETECTED! ");
        Serial.print(motionPercentage, 2);
        Serial.print("% (");
        Serial.print(motionCount);
        Serial.print("/");
        Serial.print(pollCount);
        Serial.println(")");

        return true;
    }

    motionPercentage = (pollCount > 0) ? (float)motionCount / pollCount * 100.0 : 0.0;

    Serial.print(" - ");
    Serial.print(motionPercentage, 2);
    Serial.print("% (");
    Serial.print(motionCount);
    Serial.print("/");
    Serial.print(pollCount);
    Serial.println(")");

    return false;
}

// Reset motion tracking counters (call after logging data)
void FED4::resetMotionCounters()
{
    motionCount = 0;
    pollCount = 0;
    motionPercentage = 0.0;
}
