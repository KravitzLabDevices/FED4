// src/FED4_Speaker.cpp
#include "FED4.h"

/**
 * Initializes the speaker and configures the I2S driver
 * return true if initialization is successful, false otherwise
 */
bool FED4::initializeSpeaker()
{
    // I2S configuration for MAX98357A
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 11025,   // Reduced from 44100 to improve responsiveness
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,     // Increased from 2 to 4 for better quality
        .dma_buf_len = 16,      // Reduced from 32 to 16 for better responsiveness
        .use_apll = true,       // Using APLL for better clock accuracy
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

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
        return false;
    }

    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK)
    {
        Serial.println("Failed to set I2S pins");
        i2s_driver_uninstall(I2S_NUM_0); // Cleanup on error
        return false;
    }

    return true;
}

/**
 * Enables or disables the speaker amplifier
 * @param enable true to enable the amplifier, false to disable it
 */
void FED4::enableAmp(bool enable)
{
    digitalWrite(AUDIO_SD, enable ? HIGH : LOW);
    if (enable)
    {
        delay(1); // stabilize amp
    }
}

/**
 * Plays a single tone at a specified frequency for a given duration
 * @param frequency Frequency of the tone in Hz
 * @param duration_ms Duration of the tone in milliseconds
 * @param amplitude Amplitude of the tone between 0.0 and 1.0 (default 0.25)
 */
void FED4::playTone(uint32_t frequency, uint32_t duration_ms, float amplitude)
{
    enableAmp(true);
    i2s_start(I2S_NUM_0);
    delay(1);
    // Generate and play tone
    const uint32_t sampleRate = 11025;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
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
    i2s_stop(I2S_NUM_0);
    enableAmp(false);
}

/**
 * Represents a tone with a frequency and duration
 */
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

    // enableAmp(true); 

    for (size_t i = 0; i < count; i++)
    {
        playTone(tones[i].frequency, tones[i].duration_ms, 0.25);
    }

    // enableAmp(false);
}

/**
 * Plays a startup sequence of tones 
 */
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

/**
 * Resets the speaker by uninstalling the I2S driver and reinitializing it
 */
void FED4::resetSpeaker()
{
    esp_err_t err = i2s_driver_uninstall(I2S_NUM_0);
    delay(50);
    if (err != ESP_OK)
    {
        Serial.println("Error uninstalling I2S driver");
        return;
    }

    initializeSpeaker();
}

/**
 * Plays a two-tone beep sequence - a lower tone (500 Hz) followed by a higher tone (800 Hz)
 */
void FED4::bopBeep(){
    playTone(500, 300, 0.25);  // Play 500 Hz for 300ms at 25% amplitude
    playTone(800, 100, 0.5);     // Play 800 Hz for 200ms at full amplitude
}

void FED4::resetJingle() { // 🎵 Power cycle jingle
    // Descending sequence to signify "powering down"
    playTone(1500, 80, 0.15);    // Starting even higher
    playTone(1300, 80, 0.15);    // First step down
    playTone(1100, 80, 0.15);  // Continuing descent
    playTone(900, 80, 0.15);   // Mid-range
    playTone(700, 80, 0.15);   // Getting lower
    playTone(500, 100, 0.15);  // Lower still
    playTone(300, 120, 0.15);  // Almost there
    playTone(200, 400, 0.15);  // Final deep note
    delay(300);                 // Longer dramatic pause
    
    // Ascending sequence to signify "powering up" 
    playTone(300, 50, 0.15);     // Quick low start
    playTone(600, 50, 0.2);     // Building up
    playTone(900, 50, 0.2);     // Getting stronger
    playTone(1200, 100, 0.2);   // Peak
    playTone(1500, 200, 0.2);   // Triumphant final note
    
    // Final flourish
    delay(500);
    playTone(1600, 500, 0.15);    // Quick high note
}

void FED4::menuJingle(){
    // Playful ascending arpeggio
    playTone(800, 80, 0.3);   // Root note
    playTone(1000, 60, 0.3);  // Major third
    playTone(1200, 40, 0.3);  // Fifth
    delay(50);                 // Brief pause
    
    // Quick descending cascade
    playTone(2000, 30, 0.25);
    playTone(1600, 30, 0.25); 
    playTone(1200, 30, 0.25);
    
    delay(30);  // Micro pause
    
    // Final cheerful flourish
    playTone(1500, 120, 0.4);  // Longer ending note
    playTone(2000, 80, 0.3);   // Quick high finish
}
    
/**
 * Plays a single low-pitched beep at 300 Hz
 */
void FED4::lowBeep(){
    playTone(300, 200, 0.2);  // Play 300 Hz for 200ms at 40% amplitude

}

/**
 * Plays a single high-pitched beep at 1000 Hz
 */
void FED4::highBeep(){
    playTone(1000, 200, 0.4); // Play 1000 Hz for 200ms at 40% amplitude

}

/**
 * Plays a single very high-pitched beep at 2000 Hz
 */
void FED4::higherBeep(){
    playTone(2000, 200, 0.4); // Play 2000 Hz for 200ms at 40% amplitude

}

/**
 * Plays a very short click sound
 * Used for immediate feedback on button presses or quick events
 */
void FED4::click(){
    playTone(1000, 8, 0.5);   
}

/**
 * Creates a smooth frequency sweep between two frequencies over a specified duration
 * @param startFreq Starting frequency in Hz (default 500)
 * @param endFreq Ending frequency in Hz (default 1500) 
 * @param duration_ms Total duration of the sweep in milliseconds (default 1000)
 * 
 * The sweep is broken into 50 discrete steps, with each step playing a tone
 * at an incrementally higher/lower frequency to create a smooth transition.
 * Each tone is played at 25% amplitude to avoid distortion.
 */
void FED4::soundSweep(uint32_t startFreq, uint32_t endFreq, uint32_t duration_ms) {
    // Create a smooth frequency sweep from startFreq to endFreq
    const int steps = 50; // Number of frequency steps
    const int freqStart = startFreq;
    const int freqEnd = endFreq;
    const int stepDuration = duration_ms / steps; // Total duration_ms divided into steps
    
    for (int i = 0; i < steps; i++) {
        int freq = freqStart + ((freqEnd - freqStart) * i / steps);
        playTone(freq, stepDuration, 0.25);
    }
}

/**
 * Generates white noise by playing random samples for a specified duration
 * @param duration_ms Duration of the noise in milliseconds (default 1000)
 * @param amplitude Amplitude of the noise between 0.0 and 1.0 (default 1.0)
 * 
 * Uses random number generation to create white noise samples that are played
 * through the speaker. Each sample is scaled by the amplitude parameter to
 * control the volume of the noise.
 */
void FED4::noise(uint32_t duration_ms, float amplitude){
    const uint32_t sampleRate = 22050;
    const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
    
    int16_t sampleBuffer[256];
    size_t samplesInBuffer = 0;

    // Use random numbers to generate white noise
    for (uint32_t i = 0; i < sampleCount; i++) {
        // Generate random value between -1 and 1
        float sample = (((float)random(0, 65536) / 32768.0f) - 1.0f) * amplitude;
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
}