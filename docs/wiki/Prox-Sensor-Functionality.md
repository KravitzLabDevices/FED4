# FED4 Prox (Proximity) Sensor Functionality

The FED4 uses a **VL53L1X time-of-flight (ToF)** distance sensor on the primary I2C bus (XSHUT on MCP pin 1). Measures approach distance to the nose port.

**API**

- **`initializeToF()`** — init ToF sensor (called from `begin()`). Returns `true` on success.
- **`prox()`** — take a single distance reading. Returns **distance in mm** (0–150), or **`-1`** on error/timeout.

**Details**

- Raw reading is offset by a **20 mm calibration** (configurable in code). Values &lt; 0 are clamped to 0; &gt; 150 mm are capped at 150.
- Used for approach detection (e.g. Pavlovian_Prox) and to exercise the primary I2C bus before secondary use (e.g. in `pollSensors()`).

See [FED4_Prox.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Prox.cpp). Hardware: [SparkFun VL53L1X](https://www.sparkfun.com/products/14722), [SparkFun Arduino library](https://github.com/sparkfun/SparkFun_VL53L1X_Arduino_Library).
