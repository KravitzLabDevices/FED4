#include "FED4.h"

bool FED4::initializeTRSS()
{
    pinMode(AUDIO_TRRS_1, OUTPUT);
    pinMode(AUDIO_TRRS_2, OUTPUT);
    pinMode(AUDIO_TRRS_3, OUTPUT);
    return true;
}

// Output a pulse to the TRRS output. Function is blocking unfortunately.
// usage examples:  outputPulse(1, 100);
//                  outputPulse(2, 200);
//                  outputPulse(3, 500);
void FED4::outputPulse(uint8_t trrs, uint8_t duration)
{
    // Serial.print("Output pulse on pin ");  // Add debug output
    // Serial.println(trrs);
    
    uint8_t pin;
    switch (trrs) {
        case 1:
            pin = AUDIO_TRRS_1;
            break;
        case 2:
            pin = AUDIO_TRRS_2;
            break;
        case 3:
            pin = AUDIO_TRRS_3;
            break;
        default:
            Serial.println("*** outputPulse function not called correctly, output pulse not sent.");
            return; // Invalid input, do nothing
    }
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(duration);
    digitalWrite(pin, LOW);
} 