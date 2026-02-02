# FED4 Magnet (Magnetometer) Functionality

The FED4 has an **MLX90393** magnetometer on the secondary I2C bus (address **0x0C**). **It is currently not used** by any built-in FED4 programs; the API is available for custom sketches.

**API**

- **`initializeMagnet()`** — init sensor (called from `begin()`). Uses **`configureMagnet()`** with default gain (5×).
- **`readMagnetData(x, y, z)`** — read magnetic field (µT) into `float` refs. Returns `true` on success.
- **`getMagnetEvent(&event)`** — raw data via `sensors_event_t`.
- **`setMagnetGain(...)`** / **`getMagnetGain()`** — configure or read gain (e.g. `MLX90393_GAIN_5X`).
- **`configureMagnet(gain)`** — set gain, resolution, oversampling, and filter (default 5× gain).

See [FED4_Magnet.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Magnet.cpp). Hardware: [Adafruit MLX90393](https://www.adafruit.com/product/4022), [Adafruit library](https://github.com/adafruit/Adafruit_MLX90393_Library).
