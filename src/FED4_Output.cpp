#include "FED4.h"

bool FED4::initializeTRRS()
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

// Initialize solenoids: set MCP expander pins as outputs, both off
bool FED4::initializeSolenoids()
{
    mcp.pinMode(EXP_SOL_1, OUTPUT);
    mcp.pinMode(EXP_SOL_2, OUTPUT);
    mcp.digitalWrite(EXP_SOL_1, LOW);
    mcp.digitalWrite(EXP_SOL_2, LOW);
    return true;
}

// Control a solenoid by number (1 or 2)
// state: true = energize, false = de-energize
void FED4::solenoid(uint8_t num, bool state)
{
    switch (num) {
        case 1:
            mcp.digitalWrite(EXP_SOL_1, state ? HIGH : LOW);
            break;
        case 2:
            mcp.digitalWrite(EXP_SOL_2, state ? HIGH : LOW);
            break;
        default:
            Serial.println("*** solenoid() called with invalid number (use 1 or 2)");
            break;
    }
}

// Initialize INT_OR ORed interrupt/wake line
// INT_OR aggregates interrupt signals from peripherals; configure as GPIO wake source
bool FED4::initializeIntOr()
{
    pinMode(INT_OR, INPUT);

    esp_err_t err = gpio_wakeup_enable((gpio_num_t)INT_OR, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        Serial.println("INT_OR: failed to enable GPIO wakeup");
        return false;
    }

    // esp_sleep_enable_gpio_wakeup() is called once in initializeButtons(); no need to repeat
    Serial.println("INT_OR wake line initialized");
    return true;
}
