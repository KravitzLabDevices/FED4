# FED4 `src/` ↔ Unit-Test Audit

Living checklist for aligning library code with [`examples/2_UnitTests/`](./).  
**Rule:** `src/` never depends on unit-test code; unit tests call library helpers.

## Skip (no accompanying unit test)

| File | Notes |
|------|--------|
| `FED4_Menu.cpp` | **Flagged** — rebuild later for new screen; silence/menu UX needs v1.7 retest |
| `FED4.cpp` | Core `run()` / `update()` / `waitUntil()` orchestration |
| `FED4_Buttons.cpp` | **Flagged** — hold→menu / silence / Mario paths need hardware retest + refactor |
| `FED4_Hublink.cpp` | Archive only |
| `FED4_Interrupts.cpp` | Demo mentions INT_OR only |
| `FED4_Score.cpp` | Pong |
| `FED4_Serial.cpp` | Status printf |
| `FED4_Timeout.cpp` | Timeout UI |
| `FED4_Power.cpp` | Power rails (no dedicated UT; SleepModes covers rails) |

## Domain order

1. Touch → 2. Sleep/power → 3. Display → 4. LEDs → 5. Audio → 6. Vitals → 7. Prox/Motion/Motor/Haptic/RTC/SD/Output → Demo integration

## Cross-cutting flags (fix when domain opens)

### P0
- **Version:** `library.properties` was `1.1.0` vs `libraryVer` / board `1.7.0` — **bumped properties to 1.7.0**
- **Ghost `*Pix` APIs** — **Removed**; status LED is `redPix` / `noPix` / `initializePixel` only
- **Phantom `friend class FED4_*`** — **Removed**
- **DISPLAY_WIDTH/HEIGHT** Sharp leftovers (144×168) — **fixed to 320×176** (TN0216)

### P1 contamination
- **Motor `jammed()`** — **soft-fail return** (no longer infinite mini-firmware loop)
- **SD** owns subject/session setters + sequence display state
- **Header comments wrong:** Speaker→Audio, ToF→Prox, solenoids not in Begin
- **Menu / jingles / Mario** — **Flagged** — `menuJingle` / `resetJingle` / `mario*` not Speaker-product audio; refactor when menu returns
- **Button map on v1.7** — **Flagged** — library still owns wake button handling; needs retest after menu rebuild

### P2 unused / ambiguous
- Dead sweep: `logTouchEvent`, `printMemoryStatus`, `majorJamClear`, `onTouchWakeUp`, `sendDisplayCommand`, `fed4TouchReadSmooth`, baseline mirrors — **Removed**
- Naming: `prox` vs `initializeToF` vs file `FED4_Prox.cpp`; `AUDIO_TRRS_*` used for digital TTL
- `displayActivityMonitor()` — kept for later unarchive; no longer switched from `updateDisplay`

## Touch domain (Phase 1) — status

| Item | Status |
|------|--------|
| Production still on Arduino `touchRead` / `uint16_t` / 20% abs | **Fixed** — NG `touch_sens` in `FED4_Touch.cpp` |
| Unit-test `FED4_TouchS3` duplicate driver | **Removed** — sketches call library helpers |
| Sleep release-wait still inlined touch math | **Fixed** — uses library rise helpers |
| Feed pellet-well `wakePad` ISR gate | **Fixed** — polls rise / `capturePoke` |
| `interpretTouch` legacy shape | **Done** — `capturePoke()` + FedPad index; shared idle refresh |
| Poke SD rows from sketches | **Done** — `waitUntil` logs `Left`/`Center`/`Right` |

## Sleep / power / sensors — status

| Item | Status |
|------|--------|
| Release-wait uses library rise helpers | **Done** |
| Touch wake re-enabled before light sleep | **Done** |
| Dead `handleTouch()` removed | **Done** |
| `FED4-SleepModes` uses library touch helpers | **Done** |
| Power rails peel | **Done** — `FED4_Power.cpp`; Sleep still calls PSV APIs; **PSV3 off during sleep** |
| `wakeUp()` sensor contamination | **Done** — sensors via `refreshSensors()` in `update()`; buttons kept in wake by design |
| 10-minute `pollSensors` gate | **Removed** — `refreshSensors()` always reads BME/battery/lux; no auto-`logData("Status")` |
| ActivityMonitor `program ==` branches in hot src | **Removed** — revisit when that sketch is unarchived |
| MIP VCOM keepalive | **Done** — LEDC ~30 Hz KEEP_ALIVE + RC_FAST (`FED4-VCOM-LEDC-Light-Sleep`) |
| LEDC vs `analogWrite` conflict | **Fixed** — STATUS_LED is digital only after VCOM LEDC start |
| Wake `Wire.begin()` bare | **Fixed** — `i2cReinitBus()` |
| Accel INT auto-enabled at begin | **Fixed** — INT_OR idle HIGH via H_LACTIVE |
| Sleep budget / early GPIO wakes | **Fixed** — single light-sleep; INT_OR wake off; button-only GPIO wake |
| Event-driven idle API | **Done** — `waitUntil` + `update()` + `FedPad` |
| PIR / `motion()` in idle path | **Deferred** — leave alone until v1.8.0 counter IC |
| STATUS_LED / PIR coupling | **Decoupled** from idle/`update()` |
| Frontlight default | **Off** — `displayLight(false)` in begin |

## Audio domain — status

| Item | Status |
|------|--------|
| I2S mono @ 48 kHz vs `FED4-Speaker` | **Aligned** — `initializeSpeaker` / `playTone` match UT |
| Amp SD enable pattern | **Aligned** — HIGH → settle → write → LOW (Speaker UT) |
| Boot `playStartup()` | **Fixed** — ignores `audioSilenced` NVS; Serial notes if silenced; PSV2 settle before init |
| Mario / menu jingles | **Flagged** — legacy; refactor with menu |
| Silence preference (NVS) | Runtime tones honor `audioSilenced`; boot clip does not |

## Begin / Feed / Display (BasicFED4 readiness)

| Item | Status |
|------|--------|
| `Wire.setTimeOut(50)` after bus begin | **Done** |
| Probe + recover for MCP/BME/lux/RTC/bat/accel/ToF | **Done** |
| `i2cBusHealthy` stole Wire pins | **Fixed** |
| Low-battery hang | **Fixed** — uses `sleep(sleepSeconds)` |
| `jammed()` infinite loop | **Fixed** — soft-fail + `dispenseError` |
| Program ENV header | **Fixed** — temp + humidity + lux |
| `startVcomLedc` / `stopVcomLedc` | **Done** |
| Full Demo dashboard in library UI | **Out of scope** — stays in `FED4-Demo-Hardware` |

## Hardware verify checklist

Flash with USB CDC On Boot enabled:

1. **FED4-Touch** — NG driver + rising counts on poke
2. **FED4-Touch-Light-Sleep-Multiple** — sleep, wake per pad
3. **FED4-VCOM-LEDC-Light-Sleep** — retest at ~30 Hz (~33 ms period through light sleep)
4. **FED4-Speaker** — Button 1/2 tones (library `playTone` contract)
5. **BasicFED4** — startup sound; left poke → CSV `Left` then feed; VCOM via LEDC in `waitUntil`
6. **FreeFeeding** — replace-when-taken; `update()` refreshes ENV without Status spam

## Programs

- **Unarchived:** [`examples/1_Programs/BasicFED4/`](../1_Programs/BasicFED4/) — `waitUntil()` / `FedPad::Left` / `feed()` / `update()`.
- **Unarchived:** [`examples/1_Programs/FreeFeeding/`](../1_Programs/FreeFeeding/) — `feed()` / `update()`.
- Other sketches remain in `0_Archive/1_Programs/` until their domains are consistent.
