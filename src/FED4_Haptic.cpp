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
    delay(duration*5);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration*5);
    }
}

void FED4::hapticSineWave(uint16_t duration_ms, uint8_t frequency_hz)
{
    // Software PWM sine wave using rapid toggling
    // Creates smooth amplitude variation by varying duty cycle
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency (1000 microseconds = 1ms per cycle)
    const float two_pi = 2.0 * M_PI;
    const float phase_increment = two_pi * frequency_hz / 1000.0; // Phase increment per millisecond
    
    unsigned long start_time = millis();
    float phase = 0.0;
    
    while ((millis() - start_time) < duration_ms) {
        // Calculate sine wave amplitude with minimum threshold for more obvious effect
        // Use 0.2 to 1.0 range instead of 0.0 to 1.0 so it never fully turns off
        float sine_value = sin(phase);
        float amplitude = (sine_value + 1.0) / 2.0; // Normalize to 0-1
        amplitude = 0.2 + (amplitude * 0.8); // Scale to 0.2-1.0 range for more obvious wave
        
        // Calculate PWM duty cycle (on time in microseconds)
        uint16_t on_time_us = (uint16_t)(amplitude * pwm_period_us);
        uint16_t off_time_us = pwm_period_us - on_time_us;
        
        // Apply PWM cycle
        if (on_time_us > 0) {
            mcp.digitalWrite(EXP_HAPTIC, HIGH);
            delayMicroseconds(on_time_us);
        }
        if (off_time_us > 0) {
            mcp.digitalWrite(EXP_HAPTIC, LOW);
            delayMicroseconds(off_time_us);
        }
        
        // Update phase based on time elapsed (approximately 1ms per cycle)
        phase += phase_increment;
        if (phase >= two_pi) {
            phase -= two_pi; // Keep phase in 0-2π range
        }
    }
    
    // Ensure pin is off at the end
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}

void FED4::hapticRamp(uint16_t duration_ms, bool rampUp)
{
    // Smooth ramp up or down using software PWM
    // Gradually increases or decreases amplitude over time
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency
    const uint16_t total_samples = (duration_ms * 1000) / pwm_period_us;
    
    unsigned long start_time = millis();
    uint16_t sample = 0;
    
    while ((millis() - start_time) < duration_ms) {
        // Calculate linear ramp (0.0 to 1.0 or 1.0 to 0.0)
        float progress = (float)sample / (float)total_samples;
        if (progress > 1.0) progress = 1.0;
        
        float amplitude = rampUp ? progress : (1.0 - progress);
        
        // Calculate PWM duty cycle
        uint16_t on_time_us = (uint16_t)(amplitude * pwm_period_us);
        uint16_t off_time_us = pwm_period_us - on_time_us;
        
        // Apply PWM cycle
        if (on_time_us > 0) {
            mcp.digitalWrite(EXP_HAPTIC, HIGH);
            delayMicroseconds(on_time_us);
        }
        if (off_time_us > 0) {
            mcp.digitalWrite(EXP_HAPTIC, LOW);
            delayMicroseconds(off_time_us);
        }
        
        sample++;
    }
    
    // Ensure pin is off at the end
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}

void FED4::hapticThrob(uint16_t duration_ms, uint8_t pulses)
{
    // Throbbing pulse pattern - smooth sine wave pulses
    // Each pulse is a full sine wave cycle
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency
    const uint16_t pulse_duration_ms = duration_ms / pulses;
    const uint16_t samples_per_pulse = (pulse_duration_ms * 1000) / pwm_period_us;
    const float two_pi = 2.0 * M_PI;
    
    for (uint8_t pulse = 0; pulse < pulses; pulse++) {
        for (uint16_t sample = 0; sample < samples_per_pulse; sample++) {
            // Calculate sine wave for this pulse (0 to 2π)
            float phase = (two_pi * sample) / samples_per_pulse;
            float amplitude = (sin(phase) + 1.0) / 2.0; // Normalize to 0-1
            
            // Calculate PWM duty cycle
            uint16_t on_time_us = (uint16_t)(amplitude * pwm_period_us);
            uint16_t off_time_us = pwm_period_us - on_time_us;
            
            // Apply PWM cycle
            if (on_time_us > 0) {
                mcp.digitalWrite(EXP_HAPTIC, HIGH);
                delayMicroseconds(on_time_us);
            }
            if (off_time_us > 0) {
                mcp.digitalWrite(EXP_HAPTIC, LOW);
                delayMicroseconds(off_time_us);
            }
        }
        
        // Small pause between pulses (except after last one)
        if (pulse < pulses - 1) {
            mcp.digitalWrite(EXP_HAPTIC, LOW);
            delay(pulse_duration_ms / 4); // Quarter pulse duration pause
        }
    }
    
    // Ensure pin is off at the end
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}
