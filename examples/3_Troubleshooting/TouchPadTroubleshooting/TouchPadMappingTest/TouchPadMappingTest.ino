/*
 * Touch Pad Mapping Test for FED4
 * 
 * This sketch helps determine the correct mapping between:
 * - Physical pad positions (LEFT, CENTER, RIGHT)
 * - ESP32 touch pad NUMs (NUM1, NUM5, NUM6)
 * - Which interrupt fires for each NUM
 * 
 * Instructions:
 * 1. Upload this sketch
 * 2. Wait for baseline calibration (10 seconds)
 * 3. Touch each physical pad (LEFT, CENTER, RIGHT) one at a time
 * 4. Record which NUM shows the biggest change for each physical pad
 * 5. Record which interrupt fires (ISR_NAME)
 * 6. Use the results to configure FED4_Pins.h and FED4_Touch.cpp
 */

#include <Arduino.h>
#include "esp_sleep.h"

// Define touch pad NUMs - these are what we're testing
#define TOUCH_NUM1 TOUCH_PAD_NUM1  // GPIO 1
#define TOUCH_NUM5 TOUCH_PAD_NUM5  // GPIO 5  
#define TOUCH_NUM6 TOUCH_PAD_NUM6  // GPIO 6

// Threshold for detecting touches (15% of baseline)
#define TOUCH_THRESHOLD 0.15

// Baselines for each NUM
uint16_t baseline1 = 0;
uint16_t baseline5 = 0;
uint16_t baseline6 = 0;

// Track which interrupt fired
volatile uint8_t triggeredISR = 0;  // 0=none, 1=NUM1, 5=NUM5, 6=NUM6

// ISR callbacks - each sets a unique identifier
void IRAM_ATTR isrNUM1() {
    triggeredISR = 1;
}

void IRAM_ATTR isrNUM5() {
    triggeredISR = 5;
}

void IRAM_ATTR isrNUM6() {
    triggeredISR = 6;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n==========================================");
    Serial.println("FED4 Touch Pad Mapping Test");
    Serial.println("==========================================");
    Serial.println("\nCalibrating baselines...");
    
    // Read initial baselines (touch subsystem auto-initializes on first touchRead)
    baseline1 = touchRead(TOUCH_NUM1);
    delay(10);
    baseline5 = touchRead(TOUCH_NUM5);
    delay(10);
    baseline6 = touchRead(TOUCH_NUM6);
    delay(10);
    
    // Take multiple readings and average for more stable baseline
    uint32_t sum1 = 0, sum5 = 0, sum6 = 0;
    for (int i = 0; i < 10; i++) {
        sum1 += touchRead(TOUCH_NUM1);
        sum5 += touchRead(TOUCH_NUM5);
        sum6 += touchRead(TOUCH_NUM6);
        delay(50);
    }
    baseline1 = sum1 / 10;
    baseline5 = sum5 / 10;
    baseline6 = sum6 / 10;
    
    Serial.printf("\nBaselines established:\n");
    Serial.printf("  NUM1 (GPIO 1): %d\n", baseline1);
    Serial.printf("  NUM5 (GPIO 5): %d\n", baseline5);
    Serial.printf("  NUM6 (GPIO 6): %d\n", baseline6);
    
    // Calculate thresholds
    uint16_t threshold1 = baseline1 * TOUCH_THRESHOLD;
    uint16_t threshold5 = baseline5 * TOUCH_THRESHOLD;
    uint16_t threshold6 = baseline6 * TOUCH_THRESHOLD;
    
    Serial.printf("\nThresholds (15%% of baseline):\n");
    Serial.printf("  NUM1: %d\n", threshold1);
    Serial.printf("  NUM5: %d\n", threshold5);
    Serial.printf("  NUM6: %d\n", threshold6);
    
    // Enable touch wake-up
    esp_sleep_enable_touchpad_wakeup();
    
    // Attach interrupts to each NUM
    touchAttachInterrupt(TOUCH_NUM1, isrNUM1, threshold1);
    touchAttachInterrupt(TOUCH_NUM5, isrNUM5, threshold5);
    touchAttachInterrupt(TOUCH_NUM6, isrNUM6, threshold6);
    
    Serial.println("\n==========================================");
    Serial.println("READY FOR TESTING");
    Serial.println("==========================================");
    Serial.println("Touch each physical pad (LEFT, CENTER, RIGHT)");
    Serial.println("Watch serial output to see:");
    Serial.println("  1. Which NUM(s) show changes");
    Serial.println("  2. Which interrupt fired (ISR)");
    Serial.println("  3. Deviation percentages");
    Serial.println("\nWaiting for touches...\n");
}

void loop() {
    // Read current touch values
    uint16_t val1 = touchRead(TOUCH_NUM1);
    uint16_t val5 = touchRead(TOUCH_NUM5);
    uint16_t val6 = touchRead(TOUCH_NUM6);
    
    // Calculate deviations from baseline
    float dev1 = abs((float)val1 / baseline1 - 1.0);
    float dev5 = abs((float)val5 / baseline5 - 1.0);
    float dev6 = abs((float)val6 / baseline6 - 1.0);
    
    // Check if any pad is above threshold
    bool touchDetected = (dev1 >= TOUCH_THRESHOLD) || 
                         (dev5 >= TOUCH_THRESHOLD) || 
                         (dev6 >= TOUCH_THRESHOLD);
    
    if (touchDetected) {
        // Software-based touch determination: find which pad changed most
        // This works around electrical crosstalk causing wrong interrupts to fire
        float maxDev = max(max(dev1, dev5), dev6);
        
        // Determine actual touch based on which pad changed most (software detection)
        uint8_t actualTouch = 0;  // 0=none, 1=NUM1, 5=NUM5, 6=NUM6
        const char* actualPadName = "NONE";
        const char* physicalPad = "UNKNOWN";
        
        if (maxDev >= TOUCH_THRESHOLD) {
            if (maxDev == dev6 && dev6 >= TOUCH_THRESHOLD) {
                actualTouch = 6;
                actualPadName = "NUM6";
                physicalPad = "LEFT";
            } else if (maxDev == dev5 && dev5 >= TOUCH_THRESHOLD) {
                actualTouch = 5;
                actualPadName = "NUM5";
                physicalPad = "CENTER";
            } else if (maxDev == dev1 && dev1 >= TOUCH_THRESHOLD) {
                actualTouch = 1;
                actualPadName = "NUM1";
                physicalPad = "RIGHT";
            }
        }
        
        // Get which interrupt fired (for comparison)
        uint8_t currentISR = triggeredISR;
        const char* isrName = "NONE";
        if (currentISR == 1) isrName = "ISR_NUM1";
        else if (currentISR == 5) isrName = "ISR_NUM5";
        else if (currentISR == 6) isrName = "ISR_NUM6";
        
        Serial.println("----------------------------------------");
        Serial.printf("TOUCH DETECTED!\n");
        Serial.printf("  Hardware ISR fired: %s (triggeredISR=%d)\n", isrName, currentISR);
        Serial.printf("  Software detection: %s (%s pad) - %.1f%% deviation\n", 
                     actualPadName, physicalPad, maxDev * 100);
        Serial.println("\n  All pad readings:");
        Serial.printf("    NUM1 (GPIO 1): val=%d (baseline=%d) dev=%.3f (%.1f%%)\n", 
                     val1, baseline1, dev1, dev1 * 100);
        Serial.printf("    NUM5 (GPIO 5): val=%d (baseline=%d) dev=%.3f (%.1f%%)\n", 
                     val5, baseline5, dev5, dev5 * 100);
        Serial.printf("    NUM6 (GPIO 6): val=%d (baseline=%d) dev=%.3f (%.1f%%)\n", 
                     val6, baseline6, dev6, dev6 * 100);
        
        // Compare ISR vs software detection
        if (actualTouch == 0) {
            Serial.println("\n  ⚠ WARNING: No pad above threshold - likely false trigger");
        } else if (actualTouch == currentISR) {
            Serial.println("\n  ✓ GOOD: ISR matches software detection (no crosstalk)");
        } else {
            Serial.printf("\n  ⚠ CROSSTALK: ISR fired for %s, but software detected %s\n", 
                         isrName, actualPadName);
            Serial.println("    Software detection will be used (more reliable)");
        }
        Serial.println("----------------------------------------\n");
        
        // Wait for touch to release
        while (touchDetected) {
            val1 = touchRead(TOUCH_NUM1);
            val5 = touchRead(TOUCH_NUM5);
            val6 = touchRead(TOUCH_NUM6);
            dev1 = abs((float)val1 / baseline1 - 1.0);
            dev5 = abs((float)val5 / baseline5 - 1.0);
            dev6 = abs((float)val6 / baseline6 - 1.0);
            touchDetected = (dev1 >= TOUCH_THRESHOLD) || 
                           (dev5 >= TOUCH_THRESHOLD) || 
                           (dev6 >= TOUCH_THRESHOLD);
            delay(10);
        }
        
        // Clear all state after touch is released
        // Reset interrupt flag
        triggeredISR = 0;
        
        // Take fresh readings to clear any stale values
        for (int i = 0; i < 5; i++) {
            touchRead(TOUCH_NUM1);  // Read to clear internal state
            touchRead(TOUCH_NUM5);
            touchRead(TOUCH_NUM6);
            delay(10);
        }
        
        // Small delay to ensure touch subsystem has stabilized
        delay(100);
        
        Serial.println("Touch released. State cleared. Ready for next touch.\n");
    }
    
    delay(50); // Small delay to prevent excessive CPU usage
}

