#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <ESP_I2S.h>  // New I2S API for ESP32 core 3.x

// Define the I2S pins
#define I2S_DATA_IN_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SD_PIN 42
#define BUTTON_1_PIN 40
#define BUTTON_2_PIN 39

unsigned long lastDebounceTime = 0;  // Last time the output pin was toggled
const unsigned long debounceDelay = 200; // Debounce delay in milliseconds

Audio audio;
I2SClass i2s;  // New I2S object for tone generation


void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
  const uint32_t sampleRate = 44100;
  const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
  const float amplitude = 0.5;
  const float twoPiF = 2.0 * M_PI * frequency;

  // Buffer to store multiple samples before writing
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


void setupI2S() {
  // Configure I2S pins
  i2s.setPins(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_IN_PIN);
  
  // Initialize I2S with new API
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("Failed to initialize I2S");
  }
}

void resetI2S() {
  i2s.end(); // Stop I2S driver
  setupI2S(); // Reinitialize I2S driver
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  delay(1000);

  if (!SD.begin(10)) {
    Serial.println("Card Mount Failed");
    return;
  }

  delay(1000);
  pinMode(I2S_SD_PIN, OUTPUT);
  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  Wire.begin();

  // Configure the I2S audio output
  audio.setPinout(I2S_BIT_CLOCK_PIN, I2S_LEFT_RIGHT_CLOCK_PIN, I2S_DATA_IN_PIN, -1);
  audio.setVolume(20); 

  // Initial I2S setup
  setupI2S();
}

void loop() {
  int button1State = digitalRead(BUTTON_1_PIN);
  int button2State = digitalRead(BUTTON_2_PIN);

  if (button1State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 1 Pressed");
    
    // Play the MP3 file
    audio.connecttoFS(SD, "/Audio/Beep.mp3");
   
    Serial.println(audio.inBufferFilled());
    Serial.println(audio.inBufferFree());
    // Serial.println(audio.inBufferSize());
    lastDebounceTime = millis();
  }

  if (button2State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 2 Pressed");
    
    delay(100);  // Give some time to stop audio
    
    audio.stopSong();

    // Reset and reinitialize I2S before generating tones
    resetI2S();
    
    // Generate the beep sound (1000 Hz tone for 250 milliseconds)
    generateSineWave(1000, 250);
    delay(100);
    generateSineWave(1500, 250);  // adjust the Hz to affect the pitch of the tone.
    delay(100);
    
    // Update the lastDebounceTime
    lastDebounceTime = millis();
  }

  audio.loop();
}
