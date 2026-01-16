#include "FED4.h"

void FED4::hapticBuzz(uint8_t duration)
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}

void FED4::hapticDoubleBuzz(uint8_t duration)
{
    for (int i = 0; i < 2; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration*2);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration*2);
    }
}

void FED4::hapticTripleBuzz(uint8_t duration)
{
    for (int i = 0; i < 3; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration*10);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration*10);
    }
}

void FED4::hapticRumble(uint16_t duration_ms)
{
    // Low frequency PWM rumble - creates a nice low rumble sensation
    // Uses 50Hz PWM frequency for deep rumble effect at 50% duty cycle
    const uint16_t rumble_freq_hz = 50; // 50Hz for low frequency rumble
    const uint16_t pwm_period_us = 1000000 / rumble_freq_hz; // 20000 microseconds (20ms) for 50Hz
    const uint8_t duty_cycle_percent = 50; // 50% duty cycle for smooth rumble
    
    const uint16_t on_time_us = (pwm_period_us * duty_cycle_percent) / 100;
    const uint16_t off_time_us = pwm_period_us - on_time_us;
    
    unsigned long start_time = millis();
    unsigned long cycles = (duration_ms * 1000) / pwm_period_us;
    
    for (unsigned long i = 0; i < cycles; i++) {
        mcp.digitalWrite(EXP_HAPTIC, HIGH);
        delayMicroseconds(on_time_us);
        mcp.digitalWrite(EXP_HAPTIC, LOW);
        delayMicroseconds(off_time_us);
        
        // Check if we've exceeded duration (accounting for timing variations)
        if ((millis() - start_time) >= duration_ms) {
            break;
        }
    }
    // Ensure pin is off at the end
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}