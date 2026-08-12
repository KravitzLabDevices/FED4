# FED4 Submodule I2C Specification

**Version:** v0.3 (draft)

This directory holds **standalone firmware** for satellite boards that communicate with FED4 over I2C. Submodule code is **not** part of the FED4 core library and does not use `FED4.h` or any `src/` APIs.

The only link between FED4 and submodules is the **I2C wire protocol** defined here. When FED4 master support is added, it will be separate FED4-side code that conforms to this spec.

Shared opcode constants and pack helpers: [`SubmoduleProtocol.h`](SubmoduleProtocol.h).

## Module layout

Portable helpers live in `examples/3_Submodules/` (not FED4 `src/`):

| Module | Role |
|--------|------|
| [`SubmoduleProtocol.h`](SubmoduleProtocol.h) | Wire format, status flags, pack/parse helpers (header-only) |
| [`SubmoduleState.h/.cpp`](SubmoduleState.h) | Slave runtime state + status byte building |
| [`SubmoduleRtc.h/.cpp`](SubmoduleRtc.h) | Wall-clock validation and `settimeofday()` |
| [`SubmoduleCommands.h/.cpp`](SubmoduleCommands.h) | Command dispatch; board supplies `SubmoduleCaptureOps` |
| [`SubmoduleI2cSlaveEsp32.h/.cpp`](SubmoduleI2cSlaveEsp32.h) | ESP32-S3 light sleep, SCL wake, I2C slave transport |
| [`SubmoduleMaster.h/.cpp`](SubmoduleMaster.h) | FED4-side wake, status read, send |

Board folders (e.g. [`SeeedStudioSense/`](SeeedStudioSense/)) provide pin maps, capture backend, and a thin `.ino`. Each sketch includes a `SubmodulePort.cpp` that `#include`s the shared `.cpp` files (Arduino IDE only compiles sources in the sketch directory).

Future submodules: copy the Seeed `SubmodulePort.cpp` pattern, implement `SubmoduleCaptureOps`, and set `SubmoduleI2cSlaveEsp32Config` pins.

## Purpose & scope

- Submodule sketches live under `examples/3_Submodules/<BoardName>/`.
- Each submodule is distinct hardware with its own pin map, sleep logic, and firmware.
- FED4 is the intended **I2C bus master** when integration is ready.
- This README is the evolving contract both sides will follow.

## Relationship to FED4

| | FED4 (master) | Submodules (slaves) |
|---|---|---|
| Role | I2C bus master (future) | I2C bus slaves |
| Code location | `src/` + FED4 examples | `examples/3_Submodules/` only |
| Hardware | FED4 board | Separate boards |
| Library dependency | FED4 library | Arduino-ESP32 core only |

Submodule firmware in this repo is **reference firmware** for version control alongside FED4 — not a library feature.

## Hardware wiring

Connect submodule to master I2C bus:

| Signal | Submodule (Seeed XIAO ESP32-S3 Sense) | Master |
|--------|---------------------------------------|--------|
| SDA | D4 (GPIO5) | SDA |
| SCL | D5 (GPIO6) | SCL |
| GND | GND | GND |
| 3V3 | 3V3 | 3V3 (or common rail) |

**FED4 main bus (integration reference only):** SDA = GPIO8, SCL = GPIO9, 100 kHz. Use external pull-ups on the shared bus; do not rely on submodule internal pull-ups when connected to FED4.

## I2C parameters

| Parameter | Value |
|-----------|-------|
| Bus speed | 100 kHz |
| Submodule 7-bit address | `0x42` |
| Addressing | 7-bit |

### Address space (shared FED4 bus)

Avoid conflicts with on-board FED4 devices:

| Address | Device |
|---------|--------|
| 0x10 | VEML7700 (light) |
| 0x19 | LIS2DH12 (accel) |
| 0x20 | MCP23017 (GPIO expander) |
| 0x29 | VL53L1X (ToF) |
| 0x36 | MAX17048 (fuel gauge) |
| 0x68 | DS3231 (RTC) |
| 0x76 | BME680 (temp/humidity) |
| **0x42** | **Submodule (this spec)** |

## Wake protocol

Submodules light-sleep to save power and wake when the master initiates communication.

### Master (FED4) requirements

The master **must** use a standard I2C transaction to wake a submodule:

1. Send **START + slave address (0x42) + write (W)** — a normal addressed write.
2. Send payload bytes (command frame; see below).
3. **Retry once** if the first transaction fails (NAK, timeout, or incomplete transfer).

**Do not** wiggle SCL or SDA without a proper START. Unpinned line toggling can disturb other devices on a shared bus (MCP23017, RTC, sensors, etc.).

### Slave (submodule) behavior

1. Light-sleep with **SCL low-level GPIO wake** (GPIO6 on Seeed Sense). ESP32-S3 supports level wake only (not edge).
2. First clock pulse pulls **SCL low** and wakes CPU (level wake — CPU exits sleep while SCL may still be low).
3. Submodule re-inits I2C slave (~20–200 ms after wake); master **polls until ACK** before payloads.
4. Master uses `wakeAndWaitForSubmodule()` in `FED4-Submodule-SeeedStudioSense` (up to 2 s).
5. Process one command per I2C write; return to light sleep after handling (immediately after CAPTURE).

**Note:** SCL low-level wake exits sleep while the master may still be clocking. The submodule waits for bus idle (or recovers stuck SDA) before `Wire.begin()`. A briefly busy bus after wake is **normal** and does not indicate bad pull-ups.

### Master interaction sequence (v0.3)

After wake and ACK poll, the master **reads status before any write**:

1. Wake + poll until ACK
2. **Read** 2 status bytes (`START + 0x42 + R`)
3. If `RTC_VALID` clear → write `SET_TIME` (once, or after submodule reset/hot-plug)
4. Write command(s) as needed (`CAPTURE_IMAGE`, etc.)

This supports hot-plugging the submodule after FED4 boot and avoids redundant time sync when RTC is already valid.

### Post-wake timing

| Event | Timing |
|-------|--------|
| I2C slave re-init | ~20–200 ms after wake (bus idle + `Wire.begin()`) |
| Master ready wait | Poll every 50 ms, up to 2 s, until address ACK |
| Serial debug reconnect | Up to 2 s after wake (development only; not part of wire protocol) |
| Master retry | After 100 ms if transaction fails |
| CAPTURE settle | Master should allow ~3 s before next wake (camera + SD write) |

Serial output is for bench debugging only. Production FED4 ↔ submodule communication is entirely over I2C.

## Command protocol (v0.3)

### Status read (before writes)

Any **read transaction** to `0x42` returns exactly **2 bytes**:

| Byte | Content |
|------|---------|
| 0 | Status flags — bits 0–3 below; bits 4–7 = protocol version nibble (`0x3` for v0.3) |
| 1 | Last capture error code (`0` = none) |

**Status byte 0 flags (bits 0–3):**

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `RTC_VALID` | Submodule RTC set via `SET_TIME`; datetime filenames allowed |
| 1 | `SD_READY` | SD detected at boot or last successful capture |
| 2 | `BUSY` | Reserved (capture is blocking; not set in v0.3) |
| 3 | — | Reserved |

**Error codes (byte 1):** see `SubmoduleProtocol.h` (`SUBMODULE_ERR_*`).

Master must request exactly 2 bytes (ESP32-S3 slave TX quirk). Constants and helpers: [`SubmoduleProtocol.h`](SubmoduleProtocol.h).

### Write commands

Each **I2C write transaction** to `0x42` is exactly **one command frame**. Fixed payload sizes per command — no length prefix.

| Cmd | Name | Total write size | Payload (after cmd byte) | Slave behavior |
|-----|------|------------------|--------------------------|----------------|
| `0x00` | RESERVED | — | — | Ignore |
| `0x01` | SET_TIME | **8 bytes** | `year u16 LE`, `month`, `day`, `hour`, `min`, `sec` | Validate; set ESP32 RTC; stay in listen window |
| `0x02` | CAPTURE_IMAGE | **1 or 3 bytes** | *(none)* or `imageId u16 LE` | Capture JPEG to SD; **return to light sleep immediately** |

Wrong-length frames are silently ignored.

### Command 0x01 — SET_TIME

```
[0x01][year_lo][year_hi][month][day][hour][min][sec]
```

- Ranges: year 2000–2099, month 1–12, day 1–31, hour 0–23, min/sec 0–59
- Submodule sets internal RTC via `settimeofday()` and sets `RTC_VALID`
- FED4 master reads DS3231 @ `0x68` on main bus and sends wall-clock fields

### Command 0x02 — CAPTURE_IMAGE

| Frame | Filename | Requirement |
|-------|----------|-------------|
| `[0x02]` only | `YYYYMMDDHHMMSS.jpg` (14 digits) | `RTC_VALID` status bit must be set; timestamp is **live system clock at capture** (not the SET_TIME snapshot) |
| `[0x02][id_lo][id_hi]` | `%05u.jpg` (e.g. `00042.jpg`) | Always allowed; `imageId` is uint16 (0–65535) |

If datetime mode is requested but RTC was never set: **silent fail** (no file written).

Camera and SD are initialized lazily on capture only. JPEG is saved to the onboard microSD on the Sense expansion board.

#### Seeed Sense microSD (SPI)

| Signal | GPIO | XIAO pin |
|--------|------|----------|
| CS | 21 | — (expansion board) |
| SCK | 7 | D8 |
| MISO | 8 | D9 |
| MOSI | 9 | D10 |

GPIO21 is the SD chip-select line. It shares the pin with the base board USER_LED — submodule firmware does not drive USER_LED.

The Sense expansion **J3 solder pad must be bridged** for onboard SD pull-ups. If SD mount fails with a card inserted, inspect J3 ([Seeed filesystem wiki](https://wiki.seeedstudio.com/xiao_esp32s3_sense_filesystem/)).

## Submodule index

| Folder | Board | I2C address | Status |
|--------|-------|-------------|--------|
| [SeeedStudioSense](SeeedStudioSense/) | Seeed XIAO ESP32-S3 Sense | 0x42 | v0.3 — status read, SET_TIME, CAPTURE, SD SPI |

**FED4 master test:** [`FED4-Submodule-SeeedStudioSense`](../2_UnitTests/FED4-Submodule-SeeedStudioSense/) — enables PSV2 for DS3231, initializes RTC if battery-lost, syncs time each cycle, sends CAPTURE commands.

## ESP32-S3 slave notes

Slave transmit (`onRequest`) returns the 2-byte status block. Keep the callback fast — no SD or camera work inside it.

- ESP32-S3 slave behavior differs from classic ESP32; master must request exactly the bytes the slave provides.
- Avoid `Serial` inside I2C callbacks — it can cause timeouts.
- See [Arduino-ESP32 I2C slave docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html).

## Changelog

### v0.3 (draft)

- Status register: 2-byte read (flags + last error) before writes; protocol version in status byte.
- Master skips `SET_TIME` when `RTC_VALID` is set (hot-plug and post-reset sync).
- Seeed Sense SD SPI fix: CS=GPIO21, explicit SPI on GPIO7/8/9.

### v0.2 (draft)

- Command protocol: `0x01` SET_TIME (8-byte wall clock), `0x02` CAPTURE_IMAGE (datetime or uint16 ID filename).
- `SubmoduleProtocol.h` shared constants and pack helpers.
- SeeedStudioSense: command dispatch, lazy camera/SD capture, early sleep after CAPTURE.
- FED4 master test updated for DS3231 + structured commands.

### v0.1 (draft)

- Initial spec: standalone submodule scope, address 0x42, 100 kHz.
- Wake: master sends START + 0x42 + W; slave wakes on SCL low during address clocks.
- No unpinned bus wiggling; master retries on failure.
- SeeedStudioSense reference sketch: light sleep, RX buffer, 2 s Serial grace, hex dump.
