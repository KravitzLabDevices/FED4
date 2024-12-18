// src/FED4_Speaker.cpp
#include "FED4.h"

void FED4::initializeSpeaker()
{
    // I2S configuration for MAX98357A
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};

    // I2S pin configuration
    i2s_pin_config_t pinConfig = {
        .bck_io_num = AUDIO_BCLK,
        .ws_io_num = AUDIO_LRCLK,
        .data_out_num = AUDIO_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE};

    // Initialize SD pin first
    pinMode(AUDIO_SD, OUTPUT);
    digitalWrite(AUDIO_SD, LOW); // Start with amp disabled

    // Initialize I2S with better error handling
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.println("Failed to install I2S driver");
        return;
    }

    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK)
    {
        Serial.println("Failed to set I2S pins");
        i2s_driver_uninstall(I2S_NUM_0); // Cleanup on error
        return;
    }
}

void FED4::enableAmp(bool enable)
{
    digitalWrite(AUDIO_SD, enable ? HIGH : LOW);
    if (enable)
    {
        delay(200); // Give amp time to stabilize
    }
}

void FED4::playTone(uint32_t frequency, uint32_t duration_ms, bool controlAmp = true)
{
    if (controlAmp)
    {
        enableAmp(true);
    }

    // Generate and play tone
    const uint32_t sampleRate = 44100;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
    const float amplitude = 0.5;
    const float twoPiF = 2.0 * M_PI * frequency;

    int16_t sampleBuffer[256];
    size_t samplesInBuffer = 0;

    for (uint32_t i = 0; i < sampleCount; i++)
    {
        float sample = amplitude * sin((twoPiF * i) / sampleRate);
        sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767);

        if (samplesInBuffer >= 256)
        {
            size_t bytes_written;
            i2s_write(I2S_NUM_0, sampleBuffer, sizeof(sampleBuffer), &bytes_written, portMAX_DELAY);
            samplesInBuffer = 0;
        }
    }

    // Write remaining samples
    if (samplesInBuffer > 0)
    {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }

    if (controlAmp)
    {
        enableAmp(false);
    }
}

struct Tone
{
    uint32_t frequency;
    uint32_t duration_ms;
};

void FED4::playTones(const Tone *tones, size_t count)
{
    if (!count)
        return;

    esp_err_t err = i2s_start(I2S_NUM_0);
    if (err != ESP_OK)
    {
        Serial.println("I2S not ready, skipping tones");
        return;
    }

    enableAmp(true);

    for (size_t i = 0; i < count; i++)
    {
        playTone(tones[i].frequency, tones[i].duration_ms, false);
    }

    enableAmp(false);
}

void FED4::playStartup()
{
    esp_err_t err = i2s_start(I2S_NUM_0);
    if (err != ESP_OK)
    {
        Serial.println("I2S not ready, skipping startup sound");
        return;
    }

    const Tone startupSequence[] = {
        {587, 100},  // D5
        {784, 100},  // G5
        {987, 200},  // B5
        {1175, 300}, // D6
        {987, 100},  // B5
        {784, 200},  // G5
        {1175, 300}  // D6
    };

    playTones(startupSequence, 7);
}

void FED4::resetSpeaker()
{
    esp_err_t err = i2s_driver_uninstall(I2S_NUM_0);
    if (err != ESP_OK)
    {
        Serial.println("Error uninstalling I2S driver");
        return;
    }

    initializeSpeaker();
}