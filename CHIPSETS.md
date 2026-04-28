# Specific chipsets used in FED4

| Subsystem | Chip / module | Notes |
|---|---|---|
| Main platform | **ESP32 (ESP32‑S3 family)** | Main MCU (library `architectures=esp32`; code notes ESP32‑S3 specifics) |
| Power / battery | **MAX17048** | LiPo fuel gauge / battery monitor |
| Timekeeping | **DS3231** | Real-time clock (RTC) |
| I/O expansion | **MCP23X17** (MCP23017 family) | GPIO expander (photogates, haptics control, ToF XSHUT, etc.) |
| Environmental sensing | **BME680** | Temperature / humidity / pressure / gas |
| Environmental sensing | **VEML7700** | Ambient light |
| Motion / presence | **STHS34PF80** | Motion (presence) sensor |
| Distance / proximity | **VL53L1X** | Time-of-flight (ToF) distance / proximity |
| Inertial (optional) | **LIS3DH** | 3-axis accelerometer (hardware present; optional in sketches) |
| Magnetic (optional) | **MLX90393** | 3-axis magnetometer (hardware present; optional in sketches) |
| Audio | **MAX98357A** | I2S audio amplifier (speaker output) |
| LEDs | **NeoPixel / WS2812-family** | RGB LEDs (front strip + status pixel) |
| Display | **Sharp Memory Display** | 144×168, SPI, 1-bit monochrome |
| Optional wireless | **Hublink (BLE)** | Radio chipset not specified in this repo |

