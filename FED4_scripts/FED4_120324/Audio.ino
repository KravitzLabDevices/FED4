// void generateSineWave(uint32_t frequency, uint32_t duration_ms) {
//   const uint32_t sampleRate = 44100;
//   const uint32_t sampleCount = (sampleRate * duration_ms) / 1000;
//   const float amplitude = 0.5;
//   const float twoPiF = 2.0 * M_PI * frequency;

//   for (uint32_t i = 0; i < sampleCount; i++) {
//     float sample = amplitude * sin((twoPiF * i) / sampleRate);
//     int16_t intSample = (int16_t)(sample * 32767);
//     size_t bytes_written;
//     i2s_write(I2S_NUM_0, &intSample, sizeof(intSample), &bytes_written, portMAX_DELAY);
//   }

//   // Clear the I2S buffer after generating the tone
//   i2s_zero_dma_buffer(I2S_NUM_0);
// }

// void setupI2S() {
//   // I2S configuration
//   i2s_config_t i2sConfig = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
//     .sample_rate = 44100,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
//     .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 8,
//     .dma_buf_len = 1024,
//     .use_apll = false,
//     .tx_desc_auto_clear = true,
//     .fixed_mclk = 0
//   };

//   // I2S pin configuration
//   i2s_pin_config_t pinConfig = {
//     .bck_io_num = I2S_BIT_CLOCK_PIN,
//     .ws_io_num = I2S_LEFT_RIGHT_CLOCK_PIN,
//     .data_out_num = I2S_DATA_IN_PIN,
//     .data_in_num = I2S_PIN_NO_CHANGE
//   };

//   // Initialize I2S driver
//   i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
//   i2s_set_pin(I2S_NUM_0, &pinConfig);
// }

// void resetI2S() {
//   i2s_driver_uninstall(I2S_NUM_0); // Uninstall I2S driver
//   setupI2S(); // Reinitialize I2S driver
// }
