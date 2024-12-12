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
        return;
    }

    pinMode(AUDIO_SD, OUTPUT);
    digitalWrite(AUDIO_SD, LOW); // off by default
}

void FED4::playTone(uint32_t frequency, uint32_t duration_ms)
{
    const uint32_t sampleRate = 44100;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
    const float amplitude = 0.5;
    const float twoPiF = 2.0 * M_PI * frequency;

    // Buffer to store multiple samples before writing
    int16_t sampleBuffer[256];
    size_t samplesInBuffer = 0;

    digitalWrite(AUDIO_SD, HIGH);
    delay(5);

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

    // Write any remaining samples
    if (samplesInBuffer > 0)
    {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    digitalWrite(AUDIO_SD, LOW);
}

void FED4::playStartup()
{
    // Play a short ascending melody
    esp_err_t err = i2s_start(I2S_NUM_0);
    if (err != ESP_OK)
    {
        Serial.println("I2S not ready, skipping startup sound");
        return;
    }
    playTone(1000, 250);
    delay(200);
    // playTone(523, 100); // C5
    // delay(50);
    // playTone(659, 100); // E5
    // delay(50);
    // playTone(784, 150); // G5
    // delay(50);
    // playTone(1047, 200); // C6
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