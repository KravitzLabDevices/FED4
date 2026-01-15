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

void FED4::hapticRampUp(uint16_t duration_ms)
{
    // Smooth ramp up using software PWM - starts slowly, accelerates at the end
    // Uses cubic curve (progress^3) to start very slowly and become more obvious
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency
    const uint16_t total_samples = (duration_ms * 1000) / pwm_period_us;
    
    unsigned long start_time = millis();
    uint16_t sample = 0;
    
    while ((millis() - start_time) < duration_ms) {
        // Calculate progress (0.0 to 1.0)
        float progress = (float)sample / (float)total_samples;
        if (progress > 1.0) progress = 1.0;
        
        // Use cubic curve (progress^3) to start very slowly and accelerate at the end
        // This makes the ramp more obvious as it progresses
        float curved_progress = progress * progress * progress;
        
        float amplitude = curved_progress;
        
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

void FED4::hapticRampDown(uint16_t duration_ms)
{
    // Smooth ramp down using software PWM - starts fast, decelerates slowly
    // Uses inverse cubic curve to decelerate slowly and become more obvious
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency
    const uint16_t total_samples = (duration_ms * 1000) / pwm_period_us;
    
    unsigned long start_time = millis();
    uint16_t sample = 0;
    
    while ((millis() - start_time) < duration_ms) {
        // Calculate progress (0.0 to 1.0)
        float progress = (float)sample / (float)total_samples;
        if (progress > 1.0) progress = 1.0;
        
        // Use inverse cubic curve (1 - (1-progress)^3) to decelerate slowly
        // This makes the ramp down more obvious as it progresses
        float inverse_progress = 1.0 - progress;
        float curved_inverse = inverse_progress * inverse_progress * inverse_progress;
        float amplitude = 1.0 - curved_inverse;
        
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
    // Throbbing pulse pattern - rises quickly, ends slowly for more obvious effect
    // Uses a curve that rises fast then decelerates slowly at the end
    const uint16_t pwm_period_us = 1000; // 1kHz PWM frequency
    const uint16_t pulse_duration_ms = duration_ms / pulses;
    const uint16_t samples_per_pulse = (pulse_duration_ms * 1000) / pwm_period_us;
    
    for (uint8_t pulse = 0; pulse < pulses; pulse++) {
        for (uint16_t sample = 0; sample < samples_per_pulse; sample++) {
            // Calculate progress through the pulse (0.0 to 1.0)
            float progress = (float)sample / (float)samples_per_pulse;
            if (progress > 1.0) progress = 1.0;
            
            // Use a curve that rises quickly then slows down at the end
            // Square root of progress makes it rise fast, then slow down
            // But we want the opposite - fast rise, slow end
            // Use: 1 - sqrt(1 - progress) which rises fast then slows
            float amplitude;
            if (progress >= 1.0) {
                amplitude = 1.0;
            } else {
                amplitude = 1.0 - sqrt(1.0 - progress);
            }
            
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
