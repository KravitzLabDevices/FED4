// src/FED4_Audio.cpp
#include "FED4.h"

/**
 * Initializes the speaker and configures the I2S driver
 * return true if initialization is successful, false otherwise
 */
bool FED4::initializeSpeaker()
{
    // Initialize SD pin first
    pinMode(AUDIO_SD, OUTPUT);
    digitalWrite(AUDIO_SD, LOW); // Start with amp disabled

    // Configure I2S pins for MAX98357A
    i2s.setPins(AUDIO_BCLK, AUDIO_LRCLK, AUDIO_DIN);
    
    // Initialize I2S with new API
    // Parameters: mode, sample_rate, bits_per_sample, channel_format
    // Using MONO mode with 48000 Hz - good balance for both tones and clicks
    // Higher rates (96000 Hz) work for clicks but cause crunchy longer tones
    if (!i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        Serial.println("Failed to initialize I2S");
        return false;
    }

    // Load audio silence state from preferences
    if (preferences.begin(PREFS_NAMESPACE, true)) {
        audioSilenced = preferences.getBool("audioSilenced", false);
        preferences.end();
        if (audioSilenced) {
            Serial.println("Audio silence state loaded from preferences: SILENCED");
        }
    }

    return true;
}

/**
 * Enables or disables the speaker amplifier
 * @param enable true to enable the amplifier, false to disable it
 */
void FED4::enableAmp(bool enable)
{
    // If audio has been silenced, don't allow re-enabling unless explicitly reset
    if (enable && audioSilenced) {
        return; // Silently ignore enable requests when silenced
    }
    
    digitalWrite(AUDIO_SD, enable ? HIGH : LOW);
    if (enable)
    {
        delay(1); // stabilize amp
    }
}

/**
 * Permanently disables the speaker amplifier and prevents it from being re-enabled
 * This function will keep the amp disabled even if enableAmp(true) is called later
 */
void FED4::silence()
{
    digitalWrite(AUDIO_SD, LOW);
    audioSilenced = true; // Set flag to prevent re-enabling
    
    // Save silence state to preferences
    if (preferences.begin(PREFS_NAMESPACE, false)) {
        preferences.putBool("audioSilenced", true);
        preferences.end();
        Serial.println("Audio silence state saved to preferences");
    }
    
    // Note: This function disables the amp permanently
    // To re-enable audio, you would need to call enableAmp(true) explicitly
}

/**
 * Re-enables the speaker amplifier by clearing the silence flag
 * This allows enableAmp(true) calls to work again
 */
void FED4::unsilence()
{
    audioSilenced = false; // Clear the silence flag
    
    // Save unsilence state to preferences
    if (preferences.begin(PREFS_NAMESPACE, false)) {
        preferences.putBool("audioSilenced", false);
        preferences.end();
        Serial.println("Audio unsilence state saved to preferences");
    }
    
    // Note: This doesn't automatically enable the amp, just allows it to be enabled
}

/**
 * Plays a single tone at a specified frequency for a given duration
 * @param frequency Frequency of the tone in Hz
 * @param duration_ms Duration of the tone in milliseconds
 * @param amplitude Amplitude of the tone between 0.0 and 1.0 (default 0.25)
 */
void FED4::playTone(uint32_t frequency, uint32_t duration_ms, float amplitude)
{
    // Bypass audioSilenced check for immediate playback (needed for click feedback)
    digitalWrite(AUDIO_SD, HIGH);
    delay(1);  // Stabilize amp
    
    // Generate and play tone
    const uint32_t sampleRate = 48000;
    const uint32_t originalSampleCount = (sampleRate * duration_ms) / 1000;
    const float twoPiF = 2.0 * M_PI * frequency;

    // Ensure minimum 256 samples (one full buffer) for reliable I2S transmission
    // I2S write is asynchronous - we need at least one full buffer queued
    // to ensure data is available when transmission starts
    const uint32_t sampleCount = (originalSampleCount < 256) ? 256 : originalSampleCount;

    int16_t sampleBuffer[256];  
    size_t samplesInBuffer = 0;

    for (uint32_t i = 0; i < sampleCount; i++)
    {
        // Generate tone sample if within original duration, otherwise pad with silence
        if (i < originalSampleCount)
        {
            float sample = amplitude * sin((twoPiF * i) / sampleRate);
            sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767);
        }
        else
        {
            sampleBuffer[samplesInBuffer++] = 0;  // Silence padding for short sounds
        }

        if (samplesInBuffer >= 256)
        {
            i2s.write((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
            samplesInBuffer = 0;
        }
    }

    // Write remaining samples - ensure all data is written before disabling amp
    if (samplesInBuffer > 0)
    {
        i2s.write((uint8_t*)sampleBuffer, samplesInBuffer * sizeof(int16_t));
    }
    
    // Keep amp enabled long enough for transmission to start
    // At 48000 Hz: 256 samples = ~5.3ms transmission time
    // Small delay ensures I2S DMA has started transmitting before disabling
    delayMicroseconds(500);
    
    digitalWrite(AUDIO_SD, LOW);  // Disable amp
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

    for (size_t i = 0; i < count; i++)
    {
        playTone(tones[i].frequency, tones[i].duration_ms, 0.25);
    }
}

/**
 * Plays a startup sequence of tones 
 */
void FED4::playStartup()
{
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
 * Resets the speaker by stopping I2S and reinitializing it
 */
void FED4::resetSpeaker()
{
    i2s.end();
    delay(50);
    initializeSpeaker();
}

/**
 * Plays a two-tone beep sequence - a lower tone (500 Hz) followed by a higher tone (800 Hz)
 */
void FED4::bopBeep(){
    playTone(1000, 300, 0.25);  
    playTone(1600, 100, 0.5);     
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
    delay(200);                 // Longer dramatic pause
    
    // Ascending sequence to signify "powering up" 
    playTone(300, 30, 0.15);     // Quick low start
    playTone(600, 30, 0.2);     // Building up
    playTone(900, 30, 0.2);     // Getting stronger
    playTone(1200, 50, 0.2);   // Peak
    playTone(1500, 100, 0.2);   // Triumphant final note
    
    // Final flourish
    delay(500);
    playTone(1600, 300, 0.15);    // Quick high note
}

void FED4::menuJingle(){
    // Playful ascending arpeggio
    playTone(800, 80, 0.3);   // Root note
    playTone(1000, 60, 0.3);  // Major third
    playTone(1200, 40, 0.3);  // Fifth
    delay(50);                // Brief pause
    
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
    playTone(500, 200, 0.4);  // Play 500 Hz for 200ms at 40% amplitude

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
 * Uses playTone() with minimum duration needed for reliability at 48000 Hz
 */
void FED4::click(){
    // Use playTone() to make a very short tone/click
    playTone(400, 30, 1);
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
            i2s.write((uint8_t*)sampleBuffer, sizeof(sampleBuffer));
            samplesInBuffer = 0;
        }
    }

    // Write any remaining samples
    if (samplesInBuffer > 0) {
        i2s.write((uint8_t*)sampleBuffer, samplesInBuffer * sizeof(int16_t));
    }
}
