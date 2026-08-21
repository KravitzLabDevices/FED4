# Specific chipsets used in FED4


| Subsystem             | Chip / module                  | Notes                                                                                         |
| --------------------- | ------------------------------ | --------------------------------------------------------------------------------------------- |
| Main platform         | **ESP32 (ESP32‑S3 family)**    | Main MCU (library `architectures=esp32`; code notes ESP32‑S3 specifics)                       |
| Power / battery       | **MAX17048**                   | LiPo fuel gauge / battery monitor                                                             |
| Timekeeping           | **DS3231**                     | Real-time clock (RTC)                                                                         |
| I/O expansion         | **MCP23X17** (MCP23017 family) | GPIO expander (photogates, haptics control, ToF XSHUT, etc.)                                  |
| Environmental sensing | **BME680**                     | Temperature / humidity / pressure / gas                                                       |
| Environmental sensing | **VEML7700**                   | Ambient light                                                                                 |
| Motion / presence     | **EKMB1107112**                | PIR motion sensor (digital GPIO, replaces STHS34PF80)                                         |
| Distance / proximity  | **VL53L1X**                    | Time-of-flight (ToF) distance / proximity                                                     |
| Inertial (optional)   | **LIS3DH**                     | 3-axis accelerometer (hardware present; optional in sketches)                                 |
| Audio                 | **MAX98357A**                  | I2S audio amplifier (speaker output)                                                          |
| LEDs                  | **WS2812-family**              | RGB front strip; single red status LED                                                        |
| Display               | **Kyocera TN0216ANVNANN-GN00** | 320×176 MIP reflective, 3-wire SPI, 1-bit monochrome; logical portrait 176×320 via rotation=3 |
| Optional wireless     | **Hublink (BLE)**              | Radio chipset not specified in this repo                                                      |


