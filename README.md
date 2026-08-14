[![Version](https://img.shields.io/badge/Version-1.7.0-blue.svg)](https://github.com/KravitzLabDevices/FED4)

<p align="center">
  <img src="https://github.com/KravitzLabDevices/FED4/blob/main/extras/images/FED4.gif?raw=true" alt="FED4 Demo" width="400"/>
</p>

**FED4** (Feeding Experimentation Device) is an open-source Arduino library for mouse operant training on board **v1.7**. Hardware details and chipsets: [`CHIPSETS.md`](CHIPSETS.md).

## Hardware features (v1.7)

- **MCU** — ESP32-S3  
- **Feeding** — stepper pellet dispenser with photogate well sensing and jam-clear paths  
- **Nose pokes** — three ESP32-S3 capacitive touch pads (left / center / right) with light-sleep wake  
- **Photogates** — four IR gates (left / center / right poke lanes + pellet / drop path)  
- **Display** — Kyocera TN0216 320×176 MIP reflective mono panel (SPI + VCOM)  
- **Storage** — microSD (SPI) for session CSV logging  
- **Clock** — DS3231 RTC  
- **Environment** — BME680 (temp / humidity / pressure / gas), VEML7700 ambient light  
- **Battery** — LiPo monitoring via MAX17048 fuel gauge  
- **Motion / proximity** — PIR (EKMB1107112); VL53L1X time-of-flight; LIS3DH accelerometer on board  
- **User I/O** — three front buttons; MCP23017 GPIO expander (rails, haptics, solenoids, display control)  
- **Feedback** — WS2812 front RGB strip, red status LED, haptic motor, MAX98357A I2S speaker amp  
- **Expansion** — TRRS / GPIO header (audio or digital); dual solenoid drivers; user expander pins; optional servo/INT  
- **Power** — switched peripheral rails (PSV2 / PSV3 via TPS22917) for sleep current control  
- **Optional wireless** — [Hublink](docs/wiki/Wireless-and-Hublink.md) BLE sync  

Library sketches stay small via an event-driven idle path: `waitUntil()` → act → `update()`.

## Examples

| Sketch                                            | Role                               |
| ------------------------------------------------- | ---------------------------------- |
| [`BasicFED4`](examples/1_Programs/BasicFED4/)     | Left poke → feed (starter program) |
| [`FreeFeeding`](examples/1_Programs/FreeFeeding/) | Replace pellet when taken          |
| [`2_UnitTests/`](examples/2_UnitTests/)           | Hardware bring-up / domain checks  |

More programs will return under [`examples/1_Programs/`](examples/1_Programs/) as domains are verified. Deeper API notes live in [`docs/wiki/`](docs/wiki/) and the [GitHub Wiki](https://github.com/KravitzLabDevices/FED4/wiki).

## In Development

Open work tracked in detail in [`examples/2_UnitTests/SRC_AUDIT.md`](examples/2_UnitTests/SRC_AUDIT.md):

- [ ] Rebuild menu + silence UX for v1.7 display  
- [ ] Retest / clean button holds (silence, menu, Mario / jingles)  
- [ ] Port remaining behavioral programs from archive → [`1_Programs`](examples/1_Programs/)  
- [ ] PIR / motion idle path (deferred to v1.8 counter IC)  
- [ ] Revisit ActivityMonitor when that sketch is unarchived  
- [ ] **Sense / camera** — TRRS TRIG + UART path in [`examples/3_Submodules/`](examples/3_Submodules/) (`FED4::senseSyncTime` / `senseTrigPulse`). Validate chamber lighting + fixed AE under strip white.  
- [ ] Characterize touch / SD card write duration to determine max touch rate  
