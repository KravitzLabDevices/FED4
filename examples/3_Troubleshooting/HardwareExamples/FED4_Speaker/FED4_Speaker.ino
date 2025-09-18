#include <Arduino.h>
#include "Wire.h"
#include <driver/i2s.h>
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
            size_t bytes_written;
            i2s_write(I2S_NUM_0, sampleBuffer, sizeof(sampleBuffer), &bytes_written, portMAX_DELAY);
            samplesInBuffer = 0;
        }
    }

    // Write any remaining samples
    if (samplesInBuffer > 0) {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
    
    // Clear the DMA buffer to prevent leftover sound
    i2s_zero_dma_buffer(I2S_NUM_0);
}

// Function to set up the I2S peripheral
void setupI2S() {
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = I2S_BIT_CLOCK_PIN,
        .ws_io_num = I2S_LEFT_RIGHT_CLOCK_PIN,
        .data_out_num = I2S_DATA_OUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pinConfig);
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
