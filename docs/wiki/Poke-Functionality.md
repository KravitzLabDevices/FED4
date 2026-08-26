# FED4 Poke (Nose-Poke) Functionality

Three nose-pokes (left / center / right) on ESP32-S3 capacitive touch pads ([`FED4_Pins.h`](../../src/FED4_Pins.h)). **Touch wakes** the device from light sleep; software resolves the pad, optionally logs, does a light UI update, and returns to the sketch.

This page covers the **`waitUntil()` poke path only** — not `fed4.feed()`.

**Sources:** [`FED4_Sleep.cpp`](../../src/FED4_Sleep.cpp), [`FED4_Touch.cpp`](../../src/FED4_Touch.cpp), [`FED4_SD.cpp`](../../src/FED4_SD.cpp), [`FED4.cpp`](../../src/FED4.cpp) (`update` modes).

**GPIO map (face-on):** Left = GPIO2, Center = GPIO1, Right = GPIO3.

---

## Program path (BasicFED4-style)

```text
loop:
  FedEvent e = waitUntil()     // sleep → wake → detect → log → update → return
  if (e is Left) feed()        // OUT OF SCOPE here
```

Inside **`waitUntil()`** (default UI interval 60 s):

```text
noPix()
startSleep()
  ├─ wait pads released (pre-sleep; not on the wake critical path)
  ├─ optional displayIndicators + refresh
  ├─ clear touch active latch; enable touch / button / timer wake
  ├─ esp_light_sleep_start()          ★ CPU stopped until poke/button/timer
  └─ classify lastWakeSource          ★ t0 = wake (timing reset)
wakeUp()
  ├─ release VCOM LEDC, PSV2/PSV3 on, delay(1)
  ├─ reclaim SPI / I2C, delay(1)
  ├─ capturePoke() if touch           ★ pad ID + hold → pokeDuration
  └─ button checks if needed
checkLateRetrieval()                  // photogate; may log LatePelletTaken
classify FedEvent (pad / button)
redPix(1) only if Touch && pad set
logData("Left"|"Center"|"Right")      // touch + pad only; skip if FED4_DIAG_SKIP_SD_LOG
update(Poke | Full)                   // Touch+pad → Poke; timer/button → Full
return e                              ★ sketch resumes (next cycle)
```

Sketches act on **`FedEvent`** (`source` / `pad`); they do **not** need to call `logData` for the poke itself. CSV ENV/battery columns use the last **`update(Full)` → `refreshSensors()`** snapshot (timer wake), not a re-poll on every poke.

**Members:** `leftTouch` / `centerTouch` / `rightTouch`, `leftCount` / `centerCount` / `rightCount`, `pokeDuration` (ms of hold after identify). Boot uses live idle characterization (`fed4TouchCharacterizePads`) unless an NVS cal is applied (see below).

**Pad identity after light sleep:** `esp_sleep_get_touchpad_wakeup_status()` is **deep-sleep-only** — unused here. Order:

1. NG `on_active` latch (`chan_id`)
2. Else `(smooth − benchmark) ≥ wakeAbs` (same model as HW `active_thresh`)
3. Else rise-vs-idle (also used awake, e.g. `feed()` well monitor)

---

## NVS touch calibration (device-local)

Guided **idle + poke-delta** calibration stored in Preferences namespace `fed4` (follows the device, not the SD card).

| Key | Meaning |
|-----|---------|
| `tchVer` | Schema version (`FED4_TOUCH_CAL_VER` = 1) |
| `tchMap` | Packed Left/Center/Right GPIO fingerprint (reject if pins remapped) |
| `tchBlob` | `Fed4TouchCal` POD: per-pad `idleMean`, `idleStd`, `touchDelta`, derived `wakeAbs` / `riseThresh` |

`wakeAbs = clamp(0.4 × touchDelta, TOUCH_CHAR_ABS_MIN, idle × TOUCH_RISE_MAX)`.

**Unit test wizard:** [`examples/2_UnitTests/FED4-Touch-Calibrate/`](../../examples/2_UnitTests/FED4-Touch-Calibrate/)

- Display + strip + speaker cues
- **BUTTON_1** confirms CLEAR (then ≥1.5 s quiet gate before sampling)
- Auto onset/sustain/release on the **prompted** pad (×2 L→C→R cycles; deltas must agree within ~20%)
- Saves + applies via `fed4TouchCalSave` / `fed4TouchCalApply`

**API:** `fed4TouchCalLoad` / `Save` / `Clear` / `Apply` / `Valid` in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h); `FED4::touchCal*()` wrappers.

**Production today:** `begin()` still runs live idle characterization only. **Future:** load NVS cal in `initializeTouch` when `tchVer` is present and valid, else fall back to live char.

---

## Latency model (wake → next cycle)

Times are **from CPU resume** after `esp_light_sleep_start` returns (**t0**), not from first contact. Contact → IRQ is extra (“Before t0”).

### Measured bench (Right poke, post–Poke-mode `update`)

Example Serial line:

```text
POKE_TIMING Right us: wake=15 wakeUp=140 preCap=8341 id=8355 capDone=147351
  class=147366 log=309338 update=414109 | gpioChan=3 mask=0xe
```

| Mark | µs (example) | Δ | Interpretation |
|------|--------------|---|----------------|
| `wake` | 15 | — | t0 |
| `wakeUp` | 140 | ~0.1 ms | Enter `wakeUp()` |
| `preCap` | 8341 | **~8.2 ms** | Rail / SPI / I2C / `delay(1)`×2 |
| `id` | 8355 | **~0.01 ms** | Pad known (`gpioChan=3` → Right) |
| `capDone` | 147351 | **~139 ms** | Hold until release (= `pokeDuration`) |
| `class` | 147366 | ≪1 ms | `FedEvent` + `redPix` |
| `log` | 309338 | **~162 ms** | SD `logData("Right")` |
| `update` | 414109 | **~105 ms** | **`FedUpdateMode::Poke`** (counters/indicators + MIP `refresh`, no sensors) |

**Wake → return ≈ 414 ms** on this trial (hold + SD dominate; identify is negligible).

Earlier **Full** `update` after poke was often **~400–500 ms** alone (sensors + full clear/redraw). Poke mode cuts that to ~MIP refresh (~100 ms here).

### Stage summary

| Stage | What happens | Typical | Notes |
|-------|----------------|---------|--------|
| **Before t0** | Pad rise, debounce, light-sleep exit | ~1–10+ ms | Not in `POKE_TIMING` |
| **Wake → `wakeUp`** | Cause classify | ~0.1–1 ms | |
| **`wakeUp` → `preCap`** | Rails, bus reclaim, `delay(1)`×2 | **~8 ms** measured | |
| **Identify (`id`)** | Latch / Δbenchmark / rise | **≪1 ms** when latch hits | `gpioChan` confirms GPIO |
| **Hold (`capDone − id`)** | Until released (max 500 ms) | **≈ pokeDuration** | Usually largest term if held |
| **`logData`** | SD append | **~50–200 ms** typical | Touch rows only |
| **`update(Poke)`** | Counters + indicators + `refresh()` | **~100 ms** order | No `refreshSensors` |
| **`update(Full)`** | Sensors + full status redraw + Hublink | **~50–300+ ms** | Timer / button / post-`feed()` |

### Cumulative picture (touch poke)

```text
contact ──HW──► t0 ──~8ms──► id (~0) ──hold──► log ──update(Poke)──► return
                              │ gpioChan        │      │    ~100ms
                              └─ detection ─────┘      └─ SD often ~100–200ms
```

- **Detection** = **`FED4_POKE_T_IDENTIFIED`** (not `waitUntil` return).
- **`pokeDuration`** starts after identify; excludes sleep-exit and bring-up.
- Sketch code after `waitUntil()` runs only after hold + log + update.

### Non-touch wakes

Timer / button: no `capturePoke` / poke `logData`; **`update(Full)`**. Marks `preCap` / `id` / `capDone` stay 0.

---

## Hardware validation (`FED4_DIAG_POKE_TIMING`)

In [`FED4.h`](../../src/FED4.h):

```cpp
#define FED4_DIAG_POKE_TIMING 1
```

On each resolved touch wake:

```text
POKE_TIMING Right us: wake=… wakeUp=… preCap=… id=… capDone=… class=… log=… update=… | gpioChan=3 mask=0xe
```

| Field | Meaning |
|-------|---------|
| `wake` … `update` | µs from t0 (see table above) |
| `gpioChan` | Touch channel used for ID (2=Left, 1=Center, 3=Right) |
| `mask` | Last NG `status_mask` snapshot (bit `N` ⇒ channel N); may show multiple bits |

**Checks:** `id − preCap` ≈ identify; `capDone − id` ≈ hold; `log − class` ≈ SD; `update − log` ≈ Poke UI; `gpioChan` matches the physical port.

Disable (`0`) for normal runs — Serial/`flush` adds a little overhead.

API: `fed4PokeTimingReset` / `Mark` / `Print` in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h).
