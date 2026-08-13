[![Version](https://img.shields.io/badge/Version-1.7.0-blue.svg)](https://github.com/KravitzLabDevices/FED4)

<p align="center">
  <img src="https://github.com/KravitzLabDevices/FED4/blob/main/extras/images/FED4.gif?raw=true" alt="FED4 Demo" width="400"/>
</p>

**FED4** (Feeding Experimentation Device) is an open-source Arduino library for mouse operant training on board **v1.7**. It handles pellet delivery, poke sensing, sleep/power, and session logging so experiment sketches stay small.

## Capabilities

- Pellet dispense with jam soft-fail and approach sensing  
- Three capacitive nose-pokes (ESP32-S3 touch) with light-sleep wake  
- Front RGB LEDs, status LED, haptic, and speaker feedback  
- ENV sensors (BME temp/humidity, lux, battery) refreshed in `update()`  
- MIP display with LEDC VCOM through light sleep  
- SD logging and optional [Hublink](docs/wiki/Wireless-and-Hublink.md) networking  
- Event-driven idle: `waitUntil()` → act → `update()`

## Examples

| Sketch | Role |
|--------|------|
| [`BasicFED4`](examples/1_Programs/BasicFED4/) | Left poke → feed (starter program) |
| [`FreeFeeding`](examples/1_Programs/FreeFeeding/) | Replace pellet when taken |
| [`2_UnitTests/`](examples/2_UnitTests/) | Hardware bring-up / domain checks |

More programs will return under [`examples/1_Programs/`](examples/1_Programs/) as domains are verified. Deeper API notes live in [`docs/wiki/`](docs/wiki/) and the [GitHub Wiki](https://github.com/KravitzLabDevices/FED4/wiki).

## In Development

Open work tracked in detail in [`examples/2_UnitTests/SRC_AUDIT.md`](examples/2_UnitTests/SRC_AUDIT.md):

- [ ] Rebuild menu + silence UX for v1.7 display  
- [ ] Retest / clean button holds (silence, menu, Mario / jingles)  
- [ ] Port remaining behavioral programs from archive → [`1_Programs`](examples/1_Programs/)  
- [ ] Remove ghost Pix / friend declarations and other dead APIs  
- [ ] PIR / motion idle path (deferred to v1.8 counter IC)  
- [ ] Revisit ActivityMonitor when that sketch is unarchived  
