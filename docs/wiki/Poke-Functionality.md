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

**Members:** `leftTouch` / `centerTouch` / `rightTouch`, `leftCount` / `centerCount` / `rightCount`, `pokeDuration` (ms of hold after identify). Boot sets idle + `wakeAbs` via live characterization (`fed4TouchCharacterizePads`).

**Pad identity after light sleep:** `esp_sleep_get_touchpad_wakeup_status()` is **deep-sleep-only** — unused here. Order in `capturePoke()`:

1. **Latch** — `on_active` `chan_id` / status_mask (who woke sleep)
2. **Confirm** — ~30 ms absolute `(smooth − idle)` vs per-pad `wakeAbs`; need 3 agreeing samples and a clear margin vs #2
3. **Arbiter** — latch==confirm → that pad; disagree → confirm; confirm-only or latch-only (quick tap) as fallbacks; neither → no poke

Pins (face-on): Left=GPIO1, Center=GPIO3, Right=GPIO2. Rise% first-cross is **not** used for sleep ID (false Left hits on high baseline).

---

## Touch numbers and `wakeAbs`

| Number | Source | Role |
|--------|--------|------|
| **Smooth** | Driver live read (`touch_pad_filter_read_smooth`; SMOOTH_OFF → equals raw) | Current count; rises on poke |
| **Bench** | Driver benchmark (`touch_pad_read_benchmark`, IIR8) | Hardware **baseline**; auto-tracks environmental drift |
| **Idle** | Software, at characterization | Software **baseline**; frozen until rechar / NVS apply |
| **`wakeAbs`** | Software → `touch_pad_set_thresh` | Hardware **threshold**: counts above Bench (also the software floor vs Idle) |

Sleep wake: `smooth − bench >= wakeAbs`. Awake / confirm / pads-released: `smooth − idle` vs `wakeAbs`. After a sleep poke both apply — hardware wakes on Bench, `capturePoke()` confirms on Idle.

Constants live in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h).

### In use today — live idle characterization

`begin()` → `initializeTouch()` → `fed4TouchCharacterizePads()`. Pads must be clear. Samples Smooth (warm 400 ms, then 64×20 ms), then:

```text
absDelta   = max(TOUCH_CHAR_SIGMA × std + TOUCH_CHAR_ABS_MARGIN, TOUCH_CHAR_ABS_MIN)
           = max(8 × std + 400, 800)
riseThresh = clamp(absDelta / idle, TOUCH_RISE_MIN, TOUCH_RISE_MAX)   // 0.005 … 0.12
wakeAbs    = idle × riseThresh
```

Absolute (not % of idle) so high-baseline pads (often Left ~220k) stay as sensitive as lower-baseline pads for similar poke capacitance. The 2 s `startSleep()` rescue re-runs this same formula only when software says pads are stuck **and** hardware agrees they are clear (`fed4TouchAllPadsHwInactive()`, and not right after a captured poke). A long hold must not rewrite Idle.

### WIP — NVS poke-delta calibration

Not used at boot. Wizard + NVS API exist; `begin()` does **not** auto-load. Future: `initializeTouch` loads `tchVer` when valid, else falls back to live char.

Guided **idle + poke-delta** stored in Preferences namespace `fed4` (follows the device, not the SD card). `fed4TouchCalDerivePad()`:

```text
touchDelta = touched − idle
wakeAbs    = clamp(TOUCH_CAL_DELTA_FRAC × touchDelta, TOUCH_CHAR_ABS_MIN, idle × TOUCH_RISE_MAX)
           = clamp(0.4 × touchDelta, 800, idle × 0.12)
riseThresh = wakeAbs / idle          // then re-clamped to 0.005 … 0.12
```

| Key | Meaning |
|-----|---------|
| `tchVer` | Schema version (`FED4_TOUCH_CAL_VER` = 1) |
| `tchMap` | Packed Left/Center/Right GPIO fingerprint (reject if pins remapped) |
| `tchBlob` | `Fed4TouchCal` POD: per-pad `idleMean`, `idleStd`, `touchDelta`, derived `wakeAbs` / `riseThresh` |

Wizard: [`examples/2_UnitTests/FED4-Touch-Calibrate/`](../../examples/2_UnitTests/FED4-Touch-Calibrate/) — BUTTON_1 confirms CLEAR (≥1.5 s quiet gate); auto onset/sustain/release on the prompted pad (×2 L→C→R; deltas within ~20%); `fed4TouchCalSave` / `Apply`. API in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h); `FED4::touchCal*()` wrappers.

---

## Driver: production (legacy `touch_pad`) vs NG (`touch_sens`) WIP

**Production today** is the ESP-IDF **legacy** `touch_pad` path in [`FED4_Touch.cpp`](../../src/FED4_Touch.cpp) (`driver/touch_sensor.h`, Arduino-ESP32 3.2.1 / IDF 5.4): IIR8 benchmark, denoise BIT4/L4, SMOOTH_OFF, debounce 1. Light-sleep latch is the legacy `on_active` ISR (`chan_id` / `status_mask`).

**NG `touch_sens` is not compiled in**, and — on Arduino-ESP32 3.2.1 / IDF 5.4 for the S3 — cannot be: `driver/touch_sens.h` ships only for the P4 in that core, not the S3. Smooth / Bench / `wakeAbs` semantics above are the legacy filter/benchmark model.

### Build 1: field-failure root cause and fixes

A field unit degraded over ~2 days (wrong pad → missed pokes, worst on Left →
fully unresponsive). A 74.7 h `_T.CSV` capture traced the whole progression:

1. **The measurement was ~10x longer than IDF recommends** (`TOUCH_MEASURE_CYCLES`
   gave Left ~9.7 ms vs. the IDF-recommended ~1 ms), leaving almost no headroom
   before a loaded pad's measurement went out of range.
2. That out-of-range measurement **stopped the FSM permanently** — the legacy
   driver's measurement-timeout interrupt was never configured or handled, so
   `Smooth` and `Bench` froze on all three pads simultaneously.
3. **The rescue characterization accepted the frozen runaway value** (a 13.8x
   jump) into `Idle` with no plausibility check, making the damage irreversible
   even if the FSM had recovered.
4. Independently, the ISR's channel latch used `touch_pad_get_current_meas_channel()`
   — which reports whichever channel the FSM is scanning *right now*, not the
   channel that went active — producing a deterministic (not random) pad skew.

Fixed in `FED4_Touch.cpp`/`FED4_Sleep.cpp`/`FED4_TouchHelpers.h`: measurement
time reduced to target ~1 ms; `touch_pad_timeout_set()`/`touch_pad_timeout_resume()`
wired up (`fed4TouchServiceTimeout()`); the ISR latches
the `status_mask` edge instead of the current-scan-channel register;
the pre-sleep release wait is bounded at
`FED4_TOUCH_RELEASE_WAIT_MS` instead of spinning forever (logged as `Stuck`/
`ReleaseWait`); and the absolute-Δ confirm no longer returns an unvoted
single-sample winner (`ConfirmAgreed`). See the tunables block at the top of
[`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h) and the `_T.CSV`
columns (`ScanPeriodUs`, `MeasUs{L,C,R}`, `TimeoutCount`, `StatusMask`,
`IsrChan`, `IsrMask`, `IsrCount`, `Peak{L,C,R}`, `ConfirmAgreed`,
`ReleaseWaitMs`). `IsrChan` is diagnostic only — pad ID is Latch + Confirm.

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
| **Identify (`id`)** | Latch + `(smooth − idle)` confirm | **≪1 ms** when latch hits | `gpioChan` confirms GPIO |
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

In [`FED4.h`](../../src/FED4.h) (production default is **0**; set to 1 and rebuild for a latency bench):

```cpp
#define FED4_DIAG_POKE_TIMING 0
```

On each resolved touch wake:

```text
POKE_TIMING Right us: wake=… wakeUp=… preCap=… id=… capDone=… class=… log=… update=… | gpioChan=3 mask=0xe
```

| Field | Meaning |
|-------|---------|
| `wake` … `update` | µs from t0 (see table above) |
| `gpioChan` | Raw `touch_pad_get_current_meas_channel()` at the last ISR entry — **diagnostic only** (see correction below); the authoritative channel map is `FED4_Pins.h`: **Left=1, Center=3, Right=2** |
| `mask` | Edge-accumulated active mask at the last consumer read (bit `N` ⇒ channel N newly active); may show multiple bits if more than one edge occurred between reads |

**Checks:** `id − preCap` ≈ identify; `capDone − id` ≈ hold; `log − class` ≈ SD; `update − log` ≈ Poke UI.

> **Correction (Build 1):** the worked example above (`gpioChan=3` labelled "Right", captured on a pre-fix build) and the legend that used to read *"2=Left, 1=Center, 3=Right"* were both wrong, and for the same reason: `gpioChan` was `touch_pad_get_current_meas_channel()`, which reports whichever channel the FSM happens to be scanning at ISR-entry time — not the channel that actually went active. Against millisecond-scale measurements the FSM has almost always already advanced to the *next* channel by the time the ISR runs, so Left(1) reported as 3(Center gone stale)/skewed, Right(2) reported as 3(Center), etc. That example is a captured instance of the bug, not a general truth about `gpioChan`. Pad identification no longer uses this register at all — `fed4TouchPadIndexFromLatchOnly()` uses the edge-accumulated `status_mask` (`touch_pad_get_status()`) instead; see `FED4_Touch.cpp`. `gpioChan`/`IsrChan` remain in the log as diagnostics only. Do not use them, or this example, to check "does `gpioChan` match the physical port" — use the CSV's `IsrMask`/`StatusMask` columns or the resolved `Pad`/`LatchPad`/`ConfirmPad` columns instead.

Disable (`0`) for normal runs — Serial/`flush` adds a little overhead.

API: `fed4PokeTimingReset` / `Mark` / `Print` in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h).

---

## Touch diagnostic log (`FED4_ENABLE_TOUCH_LOG`)

Logs Smooth / Bench / Idle / `wakeAbs` side by side. Does **not** change calibration (`calibrateTouchSensors()`, 2 s rescue char, NVS). Library flag in [`FED4.h`](../../src/FED4.h) — rebuild required; an `.ino` `#define` does not reach library sources. Production default is **0** (heartbeat `_T.CSV` is a diagnostic campaign). Schema, `logTouch()`, and row types stay in the code.

```cpp
#define FED4_ENABLE_TOUCH_LOG 0
```

File pair (same suffix): `/FED4_<id>_<date>_<NN>.CSV` behavioral, `/FED4_<id>_<date>_<NN>_T.CSV` touch. Heartbeats ~1440 SD appends/day at 60 s; poke rows add a second append on the wake path.

| `RowType` | When |
|-----------|------|
| `BootChar` | After `logData("Startup")` |
| `Heartbeat` | Timer wake (idle sampled while quiet) |
| `Poke` | Touch + resolved pad (`PeakSmooth`, `PokeDuration`) |
| `TouchMiss` | Touch + no pad (behavioral CSV writes nothing) |
| `Rechar` | After `startSleep()` 2 s rescue (only if HW agrees pads are clear and this wake was not a poke) |
| `Stuck` | Pre-sleep release wait hit `FED4_TOUCH_RELEASE_WAIT_MS` |
| `ReleaseWait` | Release wait slow but pads released before the hard cap |

`Mode` is `LightSleep` or `Awake` (`touchLogMode` before `begin()`). Columns: identity, `Pad` / `LatchPad` / `ConfirmPad`, Smooth / Bench / Idle / Std, RiseThresh / WakeAbs, PeakSmooth / PokeDuration, RecharCount, ProxMm, ENV/battery (stale on `Poke` rows — last `update(Full)`), WakeCount. `LatchPad` is 0 on the awake arm. `ProxMm` is `-1` except Heartbeat / Rechar.

| Sketch | Arm | Task on display |
|--------|-----|-----------------|
| [`TouchDriftLog_Sleep`](../../examples/3_Troubleshooting/TouchDriftLog_Sleep/) | `waitUntil(60)`, `Mode=LightSleep` | **SleepDrift** |
| [`TouchDriftLog_Awake`](../../examples/3_Troubleshooting/TouchDriftLog_Awake/) | Never sleeps; software `(smooth − idle)` pokes, `Mode=Awake` | **AwakeDrift** |

Firmware identity (`v1.7.1`) is on the display footer and in CSV `LibraryVer`. Bump when `src/` changes — [firmware flash tracker](../firmware/README.md).

[`extras/analysis/fed4_touch_analysis.py`](../../extras/analysis/fed4_touch_analysis.py): `python fed4_touch_analysis.py /path/to/sd_dumps -o out/`

| Observation | Conclusion |
|-------------|------------|
| `Idle` flat, `Bench` rising, pokes dying | Hardware tracking absorbed occupant/drift |
| `Idle` jumps at `Rechar`, pokes die after | Recalibrated with a mouse nearby |
| Baselines stable, `PeakSmooth − Idle` shrinking | Sensitivity loss (coupling, debris) |
| Awake healthy, sleep dying | Light-sleep / benchmark / latch — not the pad |
| High `TouchMiss` | Identification failing, not detection |
