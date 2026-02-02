# FED4 Accelerometer Functionality

The FED4 has a **LIS3DH** accelerometer on the secondary I2C bus (address **0x19**). **It is currently not used** by any built-in FED4 programs; the API is available for custom sketches.

**API**

- **`initializeAccel()`** — init sensor (called from `begin()`). Default: ±2g, 50 Hz, high-resolution (12-bit).
- **`readAccel(x, y, z)`** — read acceleration in **m/s²**, remapped to device axes (x = left/right, y = forward/back, z = up/down).
- **`getAccelEvent(&event)`** — raw chip axes via `sensors_event_t`.
- **`setAccelRange(...)`**, **`setAccelDataRate(...)`**, **`setAccelPerformanceMode(...)`** — configure range (±2/4/8/16 g), rate (1–400 Hz), and mode (low-power / normal / high-res).
- **`accelDataReady()`** — `true` when new data is available.

See [FED4_Accel.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Accel.cpp). Example: [FED4-Accel-Test](https://github.com/KravitzLabDevices/FED4/tree/main/examples/3_Troubleshooting/HardwareExamples/FED4-Accel-Test).
