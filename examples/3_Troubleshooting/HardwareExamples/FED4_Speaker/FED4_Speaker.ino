#include <Arduino.h>
#include "Wire.h"
#include <ESP_I2S.h>  // New I2S API for ESP32 core 3.x
#include <cmath> // For M_PI and sin

// Define I2S and Button pins
#define I2S_DATA_OUT_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SHUTDOWN_PIN 42
#define BUTTON_1_PIN 40
#define BUTTON_2_PIN 39

// Debounce variables
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

I2SClass i2s;  // New I2S object for tone generation

// Function to generate a sine wave tone
void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
    const uint32_t sampleRate = 44100;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
    const float amplitude = 0.5;
    const float twoPiF = 2.0 * M_PI * frequency;

    int16_t sampleBuffer[256];
    size_t samplesInBuffer = 0;

    for (uint32_t i = 0; i < sampleCount; i++) {
        float sample = amplitude * sin((twoPiF * i) / sampleRate);
        sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767);
        
        if (samplesInBuffer >= 256) {
            i2s.write((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
            samplesInBuffer = 0;
        }
    }

    // Write any remaining samples
    if (samplesInBuffer > 0) {
        i2s.write((uint8_t*)sampleBuffer, samplesInBuffer * sizeof(int16_t));
    }
}

// Function to set up the I2S peripheral
void setupI2S() {
    // Configure I2S pins
    i2s.setPins(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_OUT_PIN);
    
    // Initialize I2S with new API
    if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.println("Failed to initialize I2S");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    // Amplifier shutdown pin setup
    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);
    
    // I2S shutdown pin setup
    pinMode(I2S_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(I2S_SHUTDOWN_PIN, HIGH);

    // Button pin setup
    pinMode(BUTTON_1_PIN, INPUT);
    pinMode(BUTTON_2_PIN, INPUT);

    // Initialize I2S
    setupI2S();
    Serial.println("I2S Initialized.");
}

void loop() {
    int button1State = digitalRead(BUTTON_1_PIN);
    int button2State = digitalRead(BUTTON_2_PIN);

    // Check Button 1
    if (button1State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
        Serial.println("Button 1 Pressed - Playing Beep Tone");
        generateSineWave(880, 200); // Play an 880 Hz tone for 200ms
        lastDebounceTime = millis();
    }

    // Check Button 2
    if (button2State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
        Serial.println("Button 2 Pressed - Playing Two Tones");
        generateSineWave(1000, 250);
        delay(100);
        generateSineWave(1500, 250);
        lastDebounceTime = millis();
    }
}
