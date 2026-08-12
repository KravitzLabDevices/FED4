# FED4 `src/` ↔ Unit-Test Audit

Living checklist for aligning library code with [`examples/2_UnitTests/`](./).  
**Rule:** `src/` never depends on unit-test code; unit tests call library helpers.

## Skip (no accompanying unit test)

| File | Notes |
|------|--------|
| `FED4_Menu.cpp` | Pause — rebuild later for new screen |
| `FED4.cpp` | Core `run()` orchestration |
| `FED4_Begin.cpp` | Full `begin()` |
| `FED4_Buttons.cpp` | No dedicated button sketch |
| `FED4_Feed.cpp` | No feed/dispense sketch (touch call sites only when Touch forces it) |
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

### P1 contamination
- **Sleep** wakes buttons + `pollSensors` + touch interpret (not sleep-only)
- **Motor `jammed()`** mini firmware loop (sleep/Hublink/sensors/display)
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
| No MIP VCOM keepalive in `FED4::startSleep` | **Flagged** — unit test has timer-chunk pattern; library sleep is timer-wake based |

## Hardware verify checklist (Touch / Sleep)

Flash with USB CDC On Boot enabled:

1. **FED4-Touch** — Serial shows NG driver line + rising counts on poke; idle baselines non-zero
2. **FED4-Touch-Light-Sleep-Multiple** — sleep, wake on each pad, strip glow, unique pad names
3. **FED4-SleepModes** — light sleep touch wake prints pad; deep sleep cycle continues
4. **FED4-Demo-Hardware** — strip + poke still work after library helper switch

## Programs

Do **not** unarchive `0_Archive/1_Programs/` until covered hardware domains are consistent. `examples/1_Programs/` remains empty for now.
