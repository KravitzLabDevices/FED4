# FED4 vs FED3 (Quick Comparison)

This page summarizes high‑level differences using the **FED4 README** and the **FED3 specifications/wiki**. Where FED4 specs are not published, the table notes “not listed.”

## At a glance

| Category | FED4 | FED3 |
|---|---|---|
| **Core purpose** | Feeding experimentation device; integrated environmental monitoring and data logging | Feeding experimentation device |
| **Nose‑pokes** | Three | Three (per FED3 wiki) |
| **Pellet dispenser** | Updated design; **dispensing is greatly improved vs FED3** | Legacy FED3 dispenser |
| **Wireless / cloud** | Optional **Hublink** (BLE → gateway → hublink.cloud), auto‑configured via `meta.json` | Not listed in FED3 specs/wiki |
| **Screen** | Sharp Memory Display, 144×168 (from FED4 code) | Sharp Memory Display (FED3 wiki) |
| **Battery** | Not listed in FED4 README | 3.7 V 4400 mAh (FED3 wiki) |
| **Weight** | Not listed in FED4 README | 295 g (FED3 wiki) |
| **Dimensions** | Not listed in FED4 README | 135×85×86 mm (FED3 wiki) |
| **Processor** | Not listed in FED4 README | Feather M0 Adalogger (FED3 wiki) |

## FED4 highlights (from FED4 README)

| Area | FED4 highlights |
|---|---|
| **Sensors** | BME680 (temp/humidity/pressure/gas), VEML7700 (ambient light), VL53L1X (ToF), STHS34PF80 (motion), DS3231 RTC, plus LIS3DH accel + MLX90393 magnetometer (hardware present; currently unused in default programs) |
| **Outputs** | Multicolor nose‑poke LEDs, audio, haptic, SD logging |
| **Wireless** | Optional Hublink; enable with `fed.useHublink = true` and it syncs automatically in `run()` |

## FED3 specs (from FED3 wiki)

| Spec | FED3 |
|---|---|
| **Weight** | 295 g |
| **Dimensions** | 135×85×86 mm |
| **Processor** | Feather M0 Adalogger |
| **Screen** | Sharp Memory Display |
| **Battery** | 3.7 V 4400 mAh (~7 days continuous) |
| **Pellets** | 20 mg precision pellets (TestDiet, Bio‑Serv, etc.) |

## Sources

- FED4 README: https://github.com/KravitzLabDevices/FED4  
- FED3 specifications (wiki): https://github.com/KravitzLabDevices/FED3/wiki/FED3-Specifications  
- FED3 library: https://github.com/KravitzLabDevices/FED3_library  
