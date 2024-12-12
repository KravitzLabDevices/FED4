#include <Arduino.h>
#include "Audio.h"
#include "Wire.h"
#include "WiFi.h"
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <driver/i2s.h>

// Define the I2S pins
#define I2S_DATA_IN_PIN 41
#define I2S_BIT_CLOCK_PIN 45
#define I2S_LEFT_RIGHT_CLOCK_PIN 48
#define I2S_SD_PIN 42
#define BUTTON_1_PIN 40
#define BUTTON_2_PIN 39

unsigned long lastDebounceTime = 0;      // Last time the output pin was toggled
const unsigned long debounceDelay = 200; // Debounce delay in milliseconds

Audio audio;

void generateSineWave(uint32_t frequency, uint32_t duration_ms)
{
  const uint32_t sampleRate = 44100;
  const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
  const float amplitude = 0.5;
  const float twoPiF = 2.0 * M_PI * frequency;

  // Buffer to store multiple samples before writing
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

  // Write any remaining samples
  if (samplesInBuffer > 0)
  {
    size_t bytes_written;
    i2s_write(I2S_NUM_0, sampleBuffer, samplesInBuffer * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  }

  // Clear the I2S buffer after generating the tone
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupI2S()
{
  // I2S configuration
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
      .bck_io_num = I2S_BIT_CLOCK_PIN,
      .ws_io_num = I2S_LEFT_RIGHT_CLOCK_PIN,
      .data_out_num = I2S_DATA_IN_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE};

  // Initialize I2S driver
  i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pinConfig);
}

void resetI2S()
{
  i2s_driver_uninstall(I2S_NUM_0); // Uninstall I2S driver
  setupI2S();                      // Reinitialize I2S driver
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  delay(1000);

  if (!SD.begin(10))
  {
    Serial.println("Card Mount Failed");
    return;
  }

  delay(1000);
  pinMode(I2S_SD_PIN, OUTPUT);
  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  Wire.begin();
  // Initial I2S setup
  setupI2S();
}

void loop()
{
  int button1State = digitalRead(BUTTON_1_PIN);
  int button2State = digitalRead(BUTTON_2_PIN);

  if (button1State == HIGH && (millis() - lastDebounceTime) > debounceDelay)
  {
    Serial.println("Button 1 Pressed");

    // Play the MP3 file
    audio.connecttoFS(SD, "/Audio/Beep.mp3");

    Serial.println(audio.inBufferFilled());
    Serial.println(audio.inBufferFree());
    Serial.println(audio.inBufferSize());
    lastDebounceTime = millis();
  }

  if (button2State == HIGH && (millis() - lastDebounceTime) > debounceDelay)
  {
    Serial.println("Button 2 Pressed");

    delay(100); // Give some time to stop audio

    audio.stopSong();

    // Reset and reinitialize I2S before generating tones
    resetI2S();

    // Generate the beep sound (1000 Hz tone for 250 milliseconds)
    generateSineWave(1000, 250);
    delay(100);
    generateSineWave(1500, 250); // adjust the Hz to affect the pitch of the tone.
    delay(100);

    // Update the lastDebounceTime
    lastDebounceTime = millis();
  }

  audio.loop();
}
