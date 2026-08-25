# FED4 Poke (Nose-Poke) Functionality

Three nose-pokes (left / center / right) on ESP32-S3 capacitive touch pads ([`FED4_Pins.h`](../../src/FED4_Pins.h)). **Touch wakes** the device from light sleep; software then identifies the pad, optionally logs, refreshes UI/sensors, and returns to the sketch.

This page covers the **`waitUntil()` poke path only** — not `fed4.feed()`.

**Sources:** [`FED4_Sleep.cpp`](../../src/FED4_Sleep.cpp), [`FED4_Touch.cpp`](../../src/FED4_Touch.cpp), [`FED4_SD.cpp`](../../src/FED4_SD.cpp).

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
  ├─ enable touch / button / timer wake
  ├─ esp_light_sleep_start()          ★ CPU stopped until poke/button/timer
  └─ classify lastWakeSource          ★ t0 = wake (timing reset)
wakeUp()
  ├─ release VCOM LEDC, PSV2/PSV3 on, delay(1)
  ├─ reclaim SPI / I2C, delay(1)
  ├─ capturePoke() if touch           ★ HW wake channel → pad; then hold → pokeDuration
  └─ button checks if needed
checkLateRetrieval()                  // photogate; may log LatePelletTaken
classify FedEvent (pad / button)
redPix(1)
logData("Left"|"Center"|"Right")      // touch only; skip if FED4_DIAG_SKIP_SD_LOG
update(Poke|Full)                     // poke: no sensors, counters/indicators only
return e                              ★ sketch resumes (next cycle)
```

Sketches act on **`FedEvent`** (`source` / `pad`); they do **not** need to call `logData` for the poke itself. CSV ENV/battery columns use the last **`update()` → `refreshSensors()`** snapshot.

**Members:** `leftTouch` / `centerTouch` / `rightTouch`, `leftCount` / `centerCount` / `rightCount`, `pokeDuration` (ms of hold sampled inside `capturePoke`). Touch **characterization** runs at init (`initializeTouch`); there is no periodic re-cal after N pokes.

**Pad identity after sleep:** light sleep can wake on any enabled channel; `esp_sleep_get_touchpad_wakeup_status()` is **deep-sleep-only** and is not used. Identity comes from the NG `on_active` latch and/or `(smooth − benchmark) ≥ wakeAbs` (same model as HW), then rise-vs-idle as last resort. Software rise alone is also used for awake callers (e.g. `feed()` well monitor).

---

## Latency model (wake → next cycle)

All times below are **from CPU resume after light sleep** (`esp_light_sleep_start` returns), **not** from first finger contact. Hardware touch → IRQ is extra (see “Before t0”).

| Stage | What happens | Expected (order of magnitude) | Notes |
|-------|----------------|-------------------------------|--------|
| **Before t0** | Pad rise, HW debounce, light-sleep exit | ~1–10+ ms | Touch scan ~`meas_interval_us` (32 µs) × charge work; debounce=1; light-sleep exit usually sub‑ms–few ms |
| **Wake → `wakeUp` enter** | Cause classify, GPIO wake restore | ~0.1–1 ms | Serial prints if left enabled add more |
| **`wakeUp` → pre-`capturePoke`** | `delay(1)`×2, SPI/I2C reclaim, amp | **≥2 ms** fixed + reclaim | Dominated by the two `delay(1)` calls |
| **`capturePoke` identify** | `on_active` latch / smooth−benchmark / rise | **~0–16 ms** | Light sleep: not `esp_sleep_get_touchpad_wakeup_status()` (deep-sleep only) |
| **`capturePoke` hold** | Poll until released (or 500 ms cap) | **≈ poke duration** | **Blocks** until release (+2 consecutive below-thresh reads). Often the largest term |
| **Classify + `redPix`** | Map flags → `FedEvent` | ≪1 ms | |
| **`logData`** | RTC + SD append | **~5–50 ms** typical; up to **~500 ms** open timeout on fault | Touches only; ENV columns are prior snapshot |
| **`update()` Poke mode** | counters/indicators + `refresh()`; **no** `refreshSensors` | **~display SPI** (often ≪ full) | Timer/button still use Full (sensors + full redraw) |
| **`update()` Full** | RTC, I2C sensors, MIP full clear+redraw, serial, Hublink | **~50–300+ ms** | Display + sensor bus usually dominate |
| **Return → sketch** | — | — | Next line in `loop` (e.g. decide on `feed`) |

### Cumulative picture (touch poke)

```text
contact ──HW──► t0(wake) ──≥2ms──► identify ──hold──► log ──update──► return
                 │                  │ ~0–8ms    │        │     │
                 │                  └─ detection┘        │     └─ next cycle
                 └────────── micros marks (diag) ───────┘
```

- **“Detection”** for behavior = pad known at **`FED4_POKE_T_IDENTIFIED`** (active latch / smooth−benchmark after sleep, else rise), not when `waitUntil` returns.
- **`pokeDuration`** starts *after* identify and ends at release — it does **not** include sleep-exit or rail bring-up.
- **`waitUntil` return** is after hold + log + full `update()`, so sketch latency ≫ identify latency whenever the animal holds the poke.

### Non-touch wakes

Timer / button: no `capturePoke` / poke `logData`; still pay `wakeUp` bring-up + `update()`. Marks `preCap` / `id` / `capDone` stay 0.

---

## Hardware validation (`FED4_DIAG_POKE_TIMING`)

In [`FED4.h`](../../src/FED4.h) set:

```cpp
#define FED4_DIAG_POKE_TIMING 1
```

Rebuild. On each **touch** `waitUntil` completion, Serial prints one line (µs from t0):

```text
POKE_TIMING Left us: wake=0 wakeUp=… preCap=… id=… capDone=… class=… log=… update=…
```

| Mark | Code point | Meaning |
|------|------------|---------|
| `wake` | right after `esp_light_sleep_start()` | **t0** — CPU awake |
| `wakeUp` | start of `wakeUp()` | |
| `preCap` | immediately before `capturePoke()` | after rail/SPI/I2C bring-up |
| `id` | pad known in `capturePoke` | **detection** (latch / Δbenchmark / rise) |
| `capDone` | `capturePoke` returned | after hold/release (or failed path skips `id`) |
| `class` | after `FedEvent` + `redPix` | |
| `log` | after poke `logData` (or skip) | |
| `update` | after `update()` | ≈ return to sketch |

**How to use**

1. Enable diag; poke Left/Center/Right; capture Serial.
2. Check `id - preCap` ≈ identify cost; `capDone - id` ≈ hold; `log - class` ≈ SD; `update - log` ≈ UI/sensors.
3. Disable (`0`) for normal runs — printing/`Serial.flush` adds a little overhead on the measured path.

API: `fed4PokeTimingReset` / `Mark` / `Print` in [`FED4_TouchHelpers.h`](../../src/FED4_TouchHelpers.h).
