/*
 * FED4 Speaker / Amplifier Test
 *
 * Plays tones on button presses.
 *
 *   I2S pins:
 *     BCLK  = 41
 *     LRCLK = 40
 *     DIN   = 42
 *   PSV2 must be enabled first (MCP pin 13).
 *   Library audio is MONO @ 48 kHz.
 *   Buttons: BUTTON_1=15, BUTTON_2=16.
 */

#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Adafruit_MCP23X17.h>
#include <cmath>
#include <FED4_Pins.h>

Adafruit_MCP23X17 mcp;
I2SClass i2s;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
  const uint32_t sampleRate = 48000;
  const uint32_t originalSampleCount = (sampleRate * duration_ms) / 1000;
  const float amplitude = 0.25;
  const float twoPiF = 2.0 * M_PI * frequency;

  // Pad short tones to one full I2S buffer (same as FED4::playTone)
  const uint32_t sampleCount = (originalSampleCount < 256) ? 256 : originalSampleCount;

  int16_t sampleBuffer[256];
  size_t samplesInBuffer = 0;

  for (uint32_t i = 0; i < sampleCount; i++) {
    if (i < originalSampleCount) {
      float sample = amplitude * sin((twoPiF * i) / sampleRate);
      sampleBuffer[samplesInBuffer++] = (int16_t)(sample * 32767);
    } else {
      sampleBuffer[samplesInBuffer++] = 0;
    }

    if (samplesInBuffer >= 256) {
      i2s.write((uint8_t *)sampleBuffer, sizeof(sampleBuffer));
      samplesInBuffer = 0;
    }
  }

  if (samplesInBuffer > 0) {
    i2s.write((uint8_t *)sampleBuffer, samplesInBuffer * sizeof(int16_t));
  }
}

void playTone(uint32_t frequency, uint32_t duration_ms) {
  mcp.digitalWrite(EXP_AMP_SD, HIGH);
  delay(1); // stabilize amp
  generateSineWave(frequency, duration_ms);
  delayMicroseconds(500); // let DMA start before cutting amp
  mcp.digitalWrite(EXP_AMP_SD, LOW);
}

bool setupAmp() {
  Wire.begin(SDA, SCL, 100000);

  if (!mcp.begin_I2C()) {
    Serial.println("Error initializing MCP23017 — cannot enable amp.");
    return false;
  }

  // Amp lives on PSV2; enable that rail first
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW); // ~ON active-low
  delay(1);

  // Amp enable line: start disabled
  mcp.pinMode(EXP_AMP_SD, OUTPUT);
  mcp.digitalWrite(EXP_AMP_SD, LOW);
  return true;
}

void setupI2S() {
  i2s.setPins(AMP_BCLK, AMP_LRCLK, AMP_DIN);

  // Match FED4::initializeSpeaker(): mono @ 48 kHz
  if (!i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Speaker Test ===");
  Serial.println("I2S: BCLK=41, LRCLK=40, DIN=42 | amp enable=MCP4 via PSV2");

  if (!setupAmp()) {
    while (1) delay(10);
  }

  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);

  setupI2S();
  Serial.println("I2S ready. Press Button 1 or 2.");
}

void loop() {
  int button1State = digitalRead(BUTTON_1);
  int button2State = digitalRead(BUTTON_2);

  if (button1State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 1 — 880 Hz beep");
    playTone(880, 200);
    lastDebounceTime = millis();
  }

  if (button2State == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    Serial.println("Button 2 — two-tone");
    playTone(1000, 250);
    delay(100);
    playTone(1500, 250);
    lastDebounceTime = millis();
  }
}
