# FED4 `src/` ↔ Unit-Test Audit

Living checklist for aligning library code with [`examples/2_UnitTests/`](./).  
**Rule:** `src/` never depends on unit-test code; unit tests call library helpers.

## Skip (no accompanying unit test)

| File | Notes |
|------|--------|
| `FED4_Menu.cpp` | Pause — rebuild later for new screen |
| `FED4.cpp` | Core `run()` orchestration (now calls `orientScreen`) |
| `FED4_Buttons.cpp` | No dedicated button sketch |
| `FED4_Hublink.cpp` | Archive only |
| `FED4_Interrupts.cpp` | Demo mentions INT_OR only |
| `FED4_Score.cpp` | Pong |
| `FED4_Serial.cpp` | Status printf |
| `FED4_Timeout.cpp` | Timeout UI |

## Domain order

1. Touch → 2. Sleep/power → 3. Display → 4. LEDs → 5. Audio → 6. Vitals → 7. Prox/Motion/Motor/Haptic/RTC/SD/Output → Demo integration

## Cross-cutting flags (fix when domain opens)

### P0
- **Version:** `library.properties` was `1.1.0` vs `libraryVer` / board `1.7.0` — **bumped properties to 1.7.0**
- **Ghost `*Pix` APIs** in `FED4.h` (declared, not implemented except `redPix`/`noPix`)
- **Phantom `friend class FED4_*`** — no such classes exist
- **DISPLAY_WIDTH/HEIGHT** Sharp leftovers (144×168) — **fixed to 320×176** (TN0216)

### P1 contamination
- **Sleep** wakes buttons + `pollSensors` + touch interpret (not sleep-only) — still flagged
- **Motor `jammed()`** — **soft-fail return** (no longer infinite mini-firmware loop)
- **Vitals `pollSensors`** also calls `prox()` / `motion()`
- **SD** owns subject/session setters + sequence display state
- **Header comments wrong:** Speaker→Audio, ToF→Prox, solenoids not in Begin

### P2 unused / ambiguous
- Dead: `logTouchEvent`, `PSV2_OFF`, `printMemoryStatus`, `majorJamClear`, … (`handleTouch` removed)
- Naming: `prox` vs `initializeToF` vs file `FED4_Prox.cpp`; `AUDIO_TRRS_*` used for digital TTL

## Touch domain (Phase 1) — status

| Item | Status |
|------|--------|
| Production still on Arduino `touchRead` / `uint16_t` / 20% abs | **Fixed** — NG `touch_sens` in `FED4_Touch.cpp` |
| Unit-test `FED4_TouchS3` duplicate driver | **Removed** — sketches call library helpers |
| Sleep release-wait still inlined touch math | **Fixed** — uses library rise helpers |
| Feed pellet-well `wakePad` ISR gate | **Fixed** — polls rise / `interpretTouch` |

## Sleep domain — status

| Item | Status |
|------|--------|
| Release-wait uses library rise helpers | **Done** |
| Touch wake re-enabled before light sleep | **Done** |
| Dead `handleTouch()` removed | **Done** |
| `FED4-SleepModes` uses library touch helpers | **Done** |
| Power rails still in `FED4_Sleep.cpp` | **Flagged** — peel later |
| `wakeUp()` still runs buttons + `pollSensors` | **Flagged** — contamination |
| MIP VCOM keepalive in `FED4::startSleep` | **Done** — 500 ms chunks (currently bypassed; see TEMP delay) |
| Wake `Wire.begin()` bare | **Fixed** — `i2cReinitBus()` |
| Light sleep bypass for bring-up | **Restored** — UT-aligned chunks + VCOM + flush; touchpad wake; no auto accel INT |
| Accel INT auto-enabled at begin | **Fixed** — was holding INT_OR low → GPIO wake every chunk, masking touch wake |
| Sleep budget credited early wakes | **Fixed** — `millis()` wall-clock deadline (SleepModes); spurious GPIO/USB no longer halves period |
| Event-driven idle API | **Done** — `waitUntil(housekeepingSeconds)` + `serviceHousekeeping()`; BasicFED4 uses 60 s |
| GPIO wake during light sleep | **Fixed** — disabled for sleep session (UT has none); buttons + software touch polled between VCOM chunks |
| Poke dots sticky / missing | **Fixed** — redraw cleared indicators before sleep; BasicFED4 refreshes UI before `feed()` clears flags |
| Identify GPIO wake conflict | **Diag** — `printInterruptStatus()`; begin clears ToF/RTC/BAT/Accel latches; pre-sleep logs if INT_OR LOW |
| INT_OR stuck LOW / ACCEL | **Fixed** — LIS2DH12 default active-HIGH INT1 idles LOW; set H_LACTIVE + clear sources; ToF active-LOW; RTC INTCN; clear BAT |
| Program/menu fonts vs Demo | **Aligned** — default GFX size 1 for body; FreeSans for short labels |
| RTC stuck at 2000-01-01 | **Fixed** — `lostPower()` + year&lt;2020 + new-compile sync (Demo pattern) |
| STATUS_LED dim always-on | **Fixed** — mirrors PIR when `useMotionSensor` (Demo); BasicFED4 enables motion |
| Startup triple-click audio | **Replaced** — Demo `startup_sound.h` PCM via early `playStartup()` after power rails |
| Header ENV vertical align | **Fixed** — Demo `HEADER_H=20` / `HEADER_TEXT_Y=5` (default font top-edge) |

## Begin / Feed / Display (BasicFED4 readiness)

| Item | Status |
|------|--------|
| `Wire.setTimeOut(50)` after bus begin | **Done** |
| Probe + recover for MCP/BME/lux/RTC/bat/accel/ToF | **Done** — optional sensors; MCP always `begin_I2C` (required) |
| `i2cBusHealthy` stole Wire pins | **Fixed** — end/check/rebind; Adafruit `Wire.begin()` without pins → `i2cReinitBus()` after device begins |
| Low-battery `startSleep()` hang (no timer) | **Fixed** — uses `sleep(sleepSeconds)` |
| `jammed()` infinite loop | **Fixed** — soft-fail + `dispenseError`; dispense loop exits |
| Program ENV header (temp only) | **Fixed** — temp + humidity + lux; battery shifted right |
| `orientScreen()` never called from `run()` | **Fixed** |
| `displayInitStatus` before framebuffer alloc | **Fixed** — null-guard draw/refresh/status (was LoadProhibited at boot) |
| Full Demo dashboard in library UI | **Out of scope** — stays in `FED4-Demo-Hardware` |

## Hardware verify checklist (Touch / Sleep / Demo / BasicFED4)

Flash with USB CDC On Boot enabled:

1. **FED4-Touch** — Serial shows NG driver line + rising counts on poke; idle baselines non-zero
2. **FED4-Touch-Light-Sleep-Multiple** — sleep, wake on each pad, strip glow, unique pad names
3. **FED4-SleepModes** — light sleep touch wake prints pad; deep sleep cycle continues
4. **FED4-Demo-Hardware** — strip + poke still work after library helper switch
5. **BasicFED4** (`examples/1_Programs/BasicFED4/`) — boot completes without I2C hang; status UI shows T/H/lux + L/C/R/Pellets; left poke → feed → display refresh → sleep; left poke wakes and feeds again

## Programs

- **Unarchived:** [`examples/1_Programs/BasicFED4/`](../1_Programs/BasicFED4/) — uses `begin("BasicFED4")` + `run()` after left-poke `feed()`.
- Other sketches remain in `0_Archive/1_Programs/` until their domains are consistent.
