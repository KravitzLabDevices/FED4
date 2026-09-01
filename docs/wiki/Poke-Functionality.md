# FED4 Poke (Nose-Poke) Functionality

Three nose-pokes (left / center / right) on ESP32-S3 capacitive touch pads ([`FED4_Pins.h`](../../src/FED4_Pins.h)). **Touch wakes** the device from light sleep; software resolves the pad, optionally logs, does a light UI update, and returns to the sketch.

This page covers the **`waitUntil()` poke path only** — not `fed4.feed()`.

**Sources:** [`FED4_Sleep.cpp`](../../src/FED4_Sleep.cpp), [`FED4_Touch.cpp`](../../src/FED4_Touch.cpp), [`FED4_SD.cpp`](../../src/FED4_SD.cpp), [`FED4.cpp`](../../src/FED4.cpp) (`update` modes).

**GPIO map (face-on):** Left = GPIO1, Center = GPIO3, Right = GPIO2.

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

**Pad identity after light sleep:** `esp_sleep_get_touchpad_wakeup_status()` is **deep-sleep-only** — unused here. Order in `capturePoke()`:

1. **Latch** — NG `on_active` `chan_id` / status_mask (who woke sleep)
2. **Confirm** — ~30 ms absolute `(smooth − idle)` vs per-pad `wakeAbs`; need 3 agreeing samples and a clear margin vs #2
3. **Arbiter** — latch==confirm → that pad; disagree → confirm; confirm-only or latch-only (quick tap) as fallbacks; neither → no poke

Pins (face-on): Left=GPIO1, Center=GPIO3, Right=GPIO2. Rise% first-cross is **not** used for sleep ID (false Left hits on high baseline).

## NVS touch calibration (device-local)

Guided **idle + poke-delta** calibration stored in Preferences namespace `fed4` (follows the device, not the SD card).

| Key | Meaning |
|-----|---------|
| `tchVer` | Schema version (`FED4_TOUCH_CAL_VER` = 1) |
| `tchMap` | Packed Left/Center/Right GPIO fingerprint (reject if pins remapped) |
| `tchBlob` | `Fed4TouchCal` POD: per-pad `idleMean`, `idleStd`, `touchDelta`, derived `wakeAbs` / `riseThresh` |

`wakeAbs = clamp(0.4 × touchDelta, TOUCH_CHAR_ABS_MIN, idle × TOUCH_RISE_MAX)`.

**Unit test wizard (IN PROGRESS / WIP):** [`examples/2_UnitTests/FED4-Touch-Calibrate/`](../../examples/2_UnitTests/FED4-Touch-Calibrate/)

- Not production-ready; wizard + NVS API exist, `begin()` does not auto-load yet
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

---

## Touch diagnostic log (`FED4_ENABLE_TOUCH_LOG`)

Instrumentation for the drift-vs-sensitivity question. **Nothing about calibration changes** — `calibrateTouchSensors()`, the 2 s rescue characterization in `startSleep()`, and NVS cal loading are untouched. The rescue path is *logged as a hypothesis under test*, not modified.

Two detection models already run concurrently and were never compared:

| Model | Rule | Baseline |
|-------|------|----------|
| **Hardware** (light-sleep wake) | `smooth − benchmark >= wakeAbs` | `benchmark` — NG IIR8, **auto-tracks drift** |
| **Software** (`fed4TouchPadsReleased`, confirm) | `smooth − idle >= riseThresh × idle` | `idle` — **frozen** at characterization |

Logging both side by side is the core measurement: `Idle` is flat by construction, so `Bench` pulling away from `Idle` *is* the drift signal.

### Enabling

In [`FED4.h`](../../src/FED4.h):

```cpp
#define FED4_ENABLE_TOUCH_LOG 1
```

> **Library flag — rebuild required.** A `#define` in the `.ino` does **not** reach library sources under the Arduino IDE. Same caveat as `FED4_ENABLE_SUBMODULE`. Diagnostic builds only: heartbeat rows add ~1440 SD appends/day at 60 s, and poke rows add a **second** SD append to the wake path (roughly doubling the ~162 ms log stage in the table above). Note this when comparing `POKE_TIMING` traces against production.

### File

`createTouchLogFile()` runs immediately after `createLogFile()` and reuses its file number, so the pair always shares a suffix:

```text
/FED4_0001_20260901_00.CSV      behavioral (unchanged schema)
/FED4_0001_20260901_00_T.CSV    touch diagnostic
```

Separate file by design — the behavioral CSV and any downstream tooling stay untouched. On write failure the touch log disables itself and returns; `logData()` owns SD hot-swap recovery.

### Row types

| `RowType` | Emitted from | Why it matters |
|-----------|--------------|----------------|
| `BootChar` | `begin()`, after `logData("Startup")` | Per-device, per-port characterization table + meta.json context |
| `Heartbeat` | `waitUntil()` on a **Timer** wake | The **only** path that samples idle when nothing is happening — this is what makes drift visible |
| `Poke` | `waitUntil()`, Touch + resolved pad | `PeakSmooth` + `PokeDuration` — the sensitivity metric |
| `TouchMiss` | `waitUntil()`, Touch + **no** pad | Primary failure signature; the behavioral CSV writes **no row at all** for these |
| `Rechar` | `waitUntil()`, after `startSleep()`'s 2 s rescue fired | Direct test of "recalibrated while a mouse was nearby" |

`Mode` is `LightSleep` or `Awake`, set by the public `touchLogMode` member **before `begin()`** so the two example sketches self-label into one schema.

### Schema

| Group | Columns |
|-------|---------|
| Identity / context | `DateTime, ElapsedSeconds, DeviceUID, LibraryVer, Program, MouseID, RowType, Mode, WakeSource` |
| Identification | `Pad, LatchPad, ConfirmPad` |
| Live signal | `SmoothL, SmoothC, SmoothR` |
| Hardware baseline | `BenchL, BenchC, BenchR` |
| Software baseline | `IdleL, IdleC, IdleR, StdL, StdC, StdR` |
| Thresholds | `RiseThreshL/C/R, WakeAbsL/C/R` |
| Poke | `PeakSmooth, PokeDuration` |
| State / covariates | `RecharCount, ProxMm, Motion, Temperature, Humidity, BatteryVoltage, BatteryPercent, WakeCount` |
| Per-device context | `Cage, RackSlot, Orientation, FrontPlate, BatterySide, PokeModule` (`BootChar` rows only) |

`DeviceUID` is `ESP.getEfuseMac()`, exactly as in the behavioral CSV — the join key across devices. Rise fractions are derived offline from `Smooth` and `Idle` rather than stored, to keep rows narrow.

**Caveats to read the data with:**

- **Environment and battery are stale on `Poke` rows.** `update(FedUpdateMode::Poke)` skips `refreshSensors()`, so `Temperature` / `Humidity` / `BatteryVoltage` / `BatteryPercent` / `Motion` are whatever the last **`update(Full)`** left cached — possibly many pokes old. `Heartbeat` rows carry the previous full update's snapshot (≤ one wake interval). Deliberately not re-polled: the poke path is latency-sensitive.
- **`ProxMm` is `-1` except on `Heartbeat` and `Rechar` rows.** `prox()` blocks up to 100 ms and is kept off the poke path.
- `LatchPad` is `0` on the awake arm — there is no interrupt latch when the device never sleeps.

### Per-device context via meta.json

No firmware schema change. Hand-add a `context` block to each device's `meta.json` ([example](../../extras/meta.json_examples/meta.json)); `getMetaValue()` returns empty for missing keys, so it is optional:

```json
"context": {
  "cage": "standard-shoebox",
  "rack_slot": "R2-C3",
  "orientation": "front-facing",
  "front_plate": "v1.7-acrylic",
  "battery_side": "left",
  "poke_module": "PM-014"
}
```

Without this, module-specific and context-dependent causes cannot be separated from device-specific ones.

### Example sketches

| Sketch | Arm |
|--------|-----|
| [`TouchDriftLog_Sleep`](../../examples/3_Troubleshooting/TouchDriftLog_Sleep/) | Production-shaped cage arm — `waitUntil(60)`, left poke feeds, `Mode=LightSleep`. Reproduces the field failure. |
| [`TouchDriftLog_Awake`](../../examples/3_Troubleshooting/TouchDriftLog_Awake/) | Physics arm — never sleeps; 1 Hz `Heartbeat` to SD, 10 Hz Serial, software-detected `Poke` rows, `Mode=Awake`. Reveals the real-time physics the interrupt path hides. |

Both write the identical schema so the analysis script concatenates them.

### Analysis

[`extras/analysis/fed4_touch_analysis.py`](../../extras/analysis/fed4_touch_analysis.py) (`pip install -r requirements.txt`) globs `*_T.CSV`, keys on `DeviceUID` and pad, and emits five panels: baseline drift, poke sensitivity, cross-device/cross-port, failure signature, and sleep-vs-awake.

```bash
python fed4_touch_analysis.py /path/to/sd_dumps -o out/
```

### Interpretation table

| Observation | Conclusion |
|-------------|------------|
| `Idle` flat, `Bench` rising, pokes dying | Hardware tracking absorbed the occupant or drift |
| `Idle` jumps at a `Rechar` row, pokes die after | Recalibration with a mouse nearby — the predicted failure mode (check `ProxMm` / `Motion` on that row) |
| Baselines stable, `PeakSmooth − Idle` shrinking | True sensitivity loss (coupling, debris, mechanical) |
| One port dead across **all** devices | FED4-wide calibration issue for that port |
| One port dead on **one** device | Poke module |
| Only fails in the cage | Context — compare the `BootChar` context columns |
| Awake arm healthy, sleep arm dying | Light-sleep, benchmark, or latch path — not the pad |
| High `TouchMiss` rate | Identification failing, not detection |

### Run protocol

1. **Bench both arms, both devices:** empty desk → finger pokes → a hand deliberately lingering near a port → battery, orientation, and front-plate variations. Capture `BootChar` for every configuration. This is the device × port × context gate **before** any animal time.
2. **Cage overnight** on the sleep arm with `context` filled in per device.
3. Pull the SD, run the script, **review all five panels before changing any threshold.**
4. Only then design the recalibration framework, using the measured `Rechar` and drift rates.
