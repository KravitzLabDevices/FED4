# FED4 Submodule TRRS UART Specification

**Version:** v1.0 (TRIG + UART)

This directory holds **standalone firmware** for satellite boards that communicate with FED4 over a 4-wire TRRS-style link (power + TRIG + DATA). Submodule code is **not** part of the FED4 core library and does not use `FED4.h` or any `src/` APIs except the mirrored wire format documented here.

**FED4 master (library):** `src/FED4_Submodule.cpp` — `FED4::senseBegin()` / `senseSyncTime()` / `senseTrigPulse()`.

## Pin map

| Signal | FED4 | Sense (XIAO ESP32-S3) |
|--------|------|------------------------|
| 3V3 / GND | connector power | connector power |
| TRIG | `AUDIO_TRRS_2` = GPIO5 | GPIO1 (D0) |
| DATA | `AUDIO_TRRS_3` = GPIO6 | GPIO2 (D1) |
| spare | `AUDIO_TRRS_1` = GPIO4 | — |

- Idle **TRIG = HIGH** (Sense `INPUT_PULLUP`); FED4 drives **LOW** to wake / fire.
- **DATA:** half-duplex UART 115200 8N1, same GPIO for RX+TX; **external pull-up to 3.3 V** required.

## Shared slave helpers

| File | Role |
|------|------|
| [`SubmoduleProtocol.h`](SubmoduleProtocol.h) | Text frames, error codes |
| [`SubmoduleState.h/.cpp`](SubmoduleState.h) | `rtcValid`, last filename |
| [`SubmoduleRtc.h/.cpp`](SubmoduleRtc.h) | `settimeofday` / live clock |
| [`SubmoduleCommands.h/.cpp`](SubmoduleCommands.h) | `T` / `R` UART dispatch |
| [`SubmoduleUartEsp32.h/.cpp`](SubmoduleUartEsp32.h) | TRIG light-sleep wake, capture-blocks-sleep, UART poll |

Board folders (e.g. [`SeeedStudioSense/`](SeeedStudioSense/)) provide pin maps, camera backend, and a thin `.ino` plus `SubmodulePort.cpp` that `#include`s the shared `.cpp` files.

## Relationship to FED4

| | FED4 (master) | Submodules (slaves) |
|---|---|---|
| Role | TRIG + UART master | TRIG wake + capture |
| Code | `src/FED4_Submodule.cpp` + `FED4::sense*` | `examples/3_Submodules/` only |

## Protocol

### Init (synchronous)

1. FED4 TRIG LOW  
2. Sense wakes, UART up, sends `RDY\n`  
3. FED4 sends `T <yyyy> <mm> <dd> <HH> <MM> <SS>\n`  
4. Sense sets RTC, replies `OK\n` (or `ERR <code>\n`)  
5. FED4 TRIG HIGH → Sense light sleep  

Until `rtcValid`, Sense **does not capture** on TRIG (UART/`T` only).

### Capture (asynchronous — fire and forget)

1. FED4: TRIG LOW for **~10 ms**, then HIGH (`senseTrigPulse`). No UART wait for capture.  
2. Sense: on TRIG active + `rtcValid`, **one** capture to `YYYYMMDDHHMMSS.jpg` (blocking). **Do not light-sleep while capturing**, even if TRIG returns HIGH.  
3. After capture: if TRIG still LOW, accept UART (`T`, `R <name>`); if HIGH, light sleep.  
4. Next HIGH→LOW after a completed cycle starts a new capture.

### UART frames (init / post-capture session)

| Line | Direction | Meaning |
|------|-----------|---------|
| `RDY` | Sense→FED4 | UART ready |
| `T yyyy mm dd HH MM SS` | FED4→Sense | SET_TIME |
| `R <filename>` | FED4→Sense | Rename last capture |
| `OK` | Sense→FED4 | Success |
| `ERR <code>` | Sense→FED4 | Failure |

Capture itself has **no** OK/ERR handshake on the wire.

## Submodule index

| Folder | Board | Status |
|--------|-------|--------|
| [SeeedStudioSense](SeeedStudioSense/) | Seeed XIAO ESP32-S3 Sense | v1.0 — TRIG capture, SET_TIME, optional rename |

**FED4 master test:** [`FED4-Submodule-SeeedStudioSense`](../2_UnitTests/FED4-Submodule-SeeedStudioSense/) — `senseSyncTime` + `senseTrigPulse`.

## Changelog

### v1.0
- Replaced I2C @ 0x42 with TRRS TRIG + half-duplex UART.
- Async TRIG pulse capture; sync SET_TIME at init.
