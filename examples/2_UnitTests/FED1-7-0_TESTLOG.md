# FED v1.7 Hardware Examples Test Log

## Notes for v1.7.1

- Re-think SD card and display power rails; sharing 3.3V2 means a lot of other stuff becomes active w/ SD card
- **MAX98357A on 3.3V2:** `I_Q` ~2.4 mA only when **SD HIGH + I2S clocks** (active). **SleepModes** drives `EXP_AMP_SD` LOW → shutdown **~0.6 µA** (`I_SHDN`). Standby (SD HIGH, no BCLK) = **~340 µA**. Consider dedicated amp rail for audio-active sketches (v1.7.1)
- **TCA4307 on 3.3V2:** ~2.5 mA `I_CC` typ to isolate RTC I2C — primary motivation to drop isolator or gate EN in v1.7.1 (RTC on main bus or new chip)
- **GP1A57HR photogate on 3.3V2:** IR LED hard-wired to rail via **1 kΩ** → **~2.5–3.0 mA per mounted gate** whenever PSV2 on; GPIO only reads open-collector output. Gate LED bias or separate PG rail for light-sleep savings (v1.7.1)
- Add more separation between RESET/BOOT buttons

## Power Profile

Baseline state model from `[FED4-SleepModes.ino](FED4-SleepModes/FED4-SleepModes.ino)` — **5 s light sleep → brief awake → 5 s deep sleep**, repeating.

### Measured (VBATT)

Ammeter on pack; SleepModes sketch unless noted. Battery life = capacity ÷ current (constant-mode, ideal — no self-discharge or Peukert).


| Mode                 | Measured | 3300 mAh | 6600 mAh | 10050 mAh |
| -------------------- | -------- | -------- | -------- | --------- |
| **Power switch off** | 4.5 µA   | ~84 yr   | ~167 yr  | ~255 yr   |
| **Startup / active** | 52.42 mA | 2.6 d    | 5.2 d    | 8.0 d     |
| **Light sleep**      | 6.82 mA  | 20 d     | 40 d     | 61 d      |
| **Deep sleep**       | 1.38 mA  | 3.3 mo   | 6.6 mo   | 10 mo     |
| **SleepModes avg**¹  | ~4.1 mA  | 34 d     | 67 d     | 102 d     |


¹ Measured 50/50 duty: (6.82 + 1.38) / 2 ≈ 4.1 mA; excludes short awake SPI bursts (~52 mA).

### Measured (VBATT) — v1.7.1 Mods

Ammeter on pack; SleepModes sketch unless noted. Battery life = capacity ÷ current (constant-mode, ideal — no self-discharge or Peukert).

**Mod 1:** Removed I2C isolator (TCA4307), tied I2C_2 to I2C, removed I2C2 pull-ups; DS3231 RTC tied to always-on **3.3 V** (main I2C bus).


| Mode                 | Measured | 3300 mAh | 6600 mAh | 10050 mAh |
| -------------------- | -------- | -------- | -------- | --------- |
| **Startup / active** | 50.24 mA | 2.7 d    | 5.5 d    | 8.3 d     |
| **Light sleep**      | 4.92 mA  | 28 d     | 56 d     | 85 d      |
| **Deep sleep**       | 1.48 mA  | 3.1 mo   | 6.2 mo   | 9.5 mo    |
| **SleepModes avg**¹  | ~3.2 mA  | 43 d     | 86 d     | 131 d     |


¹ Measured 50/50 duty: (4.92 + 1.48) / 2 ≈ 3.2 mA; excludes short awake SPI bursts (~50 mA).

**vs v1.7 (stock):** Light sleep **−1.9 mA** (6.82 → 4.92) — consistent with removing TCA4307 ~2.5 mA `I_CC` while PSV2 remains on. Deep sleep **+0.1 mA** (1.38 → 1.48); within measurement noise. RTC on 3.3 V eliminates isolator back-power when PSV2 is off.

**Light sleep budget — reconciled (bench: 1× GP1A57HR mounted on 3.3V2, 1 kΩ LED resistor):**

| Bucket | Stock v1.7 (~6.82 mA) | v1.7.1 Mod 1 (~4.92 mA) |
| ------ | --------------------- | ------------------------ |
| **ESP32-S3** light sleep + touch wake | ~1.1 mA | ~1.1 mA |
| **Always-on 3.3 V** (MCP, sensors shutdown, MIP, RTC¹) | ~0.5–0.8 mA | ~0.5–0.8 mA |
| **TCA4307** (`I_CC`, PSV2 on) | ~2.5 mA | 0 (removed) |
| **GP1A57HR** ×1 (3.3V2, LED always on) | **~2.5–3.0 mA** | **~2.5–3.0 mA** |
| Amp + other PSV2 leakage | ~0.1 mA | ~0.1 mA |
| **Sum (typ)** | **~6.7–7.5 mA** | **~4.2–5.0 mA** |

¹ v1.7.1 Mod 1: DS3231 on always-on 3.3 V (~100–200 µA included in always-on bucket). Stock: RTC on PSV2 behind TCA4307.

PSV2 stays **on** during light sleep → photogate IR LED draws whenever the rail is enabled. Driving photogate GPIO LOW does not cut LED current (LED tied directly to 3.3V2).

**Deep sleep PSV2-off anomaly (bench, SleepModes):**


| Condition                     | Deep sleep VBATT           | Notes                                              |
| ----------------------------- | -------------------------- | -------------------------------------------------- |
| PSV2 **off** (intended)       | **~17 mA** observed        | ~+10 mA vs keeping PSV2 on — not SD-card-specific  |
| PSV2 **on** during deep sleep | ~light sleep + **~1 mA** | ESP32 mode delta only; 3.3V2 loads behave normally |
| SDIO teardown alone           | No improvement             | Rules out SD socket back-power as sole cause       |


Turning **PSV2 off** should remove ~2.5 mA (TCA4307, stock) plus small PSV2 leakage and yield ~1.4 mA; a bench solder issue once showed ~17 mA instead. Likely **GPIO/I2C back-power into the unpowered 3.3V2 domain**:


| Path                              | Always-on source                                | PSV2 load at risk                          |
| --------------------------------- | ----------------------------------------------- | ------------------------------------------ |
| Photogate GPIO 14/17/18/37        | ESP32 I/O (INPUT_PULLUP in production sketches) | PG LED + receiver front-ends on 3.3V2      |
| I2S GPIO 40/41/42                 | ESP32 I/O after audio init                      | MAX98357A on 3.3V2                         |
| Main I2C SDA/SCL pull-ups         | MCP + sensors on 3.3 V                          | **TCA4307 + DS3231** when isolator VCC off |
| MCP EXP_AMP_SD / haptic / sol LOW | MCP23017 on 3.3 V                               | PSV2 peripherals with VDD off              |


`FED4-SleepModes` now drives photogate + I2S lines **LOW** and quiesces SDIO before `PSV2_OFF()` — retest deep sleep. Remaining suspect if still high: **main I2C → TCA4307** (architectural; v1.7.1 RTC bus change).

**PSV2 light vs deep delta (6.82 − 1.38 ≈ 5.4 mA) — reconciled (PSV2 off working correctly):**

SleepModes holds **`EXP_AMP_SD` LOW** before sleep → MAX98357A in **shutdown** (~0.6 µA), not 2.4 mA active `I_Q`.


| 3.3V2 load (PSV2 on, light sleep)     | Typ current     | Deep sleep (PSV2 off) |
| ------------------------------------- | --------------- | --------------------- |
| **TCA4307** I2C isolator (`I_CC`)     | **~2.5 mA** (stock) | 0                 |
| **GP1A57HR** photogate ×1 (1 kΩ LED)  | **~2.5–3.0 mA** | 0                     |
| **MAX98357A** amp (`EXP_AMP_SD` LOW)  | **~0.6 µA**     | 0                     |
| DS3231 RTC (stock PSV2) + SD/sol/haptic | ~0.1–0.3 mA   | 0                     |
| **PSV2 subtotal** (1 PG mounted)      | **~5.1–5.8 mA** (stock) / **~2.6–3.3 mA** (Mod 1) | 0 |


Plus **ESP32-S3** light sleep + touch (**~1.1 mA**) and **always-on 3.3 V** loads (MCP, sensors, MIP ≈ **0.5–0.8 mA**) → stock **~6.8 mA** and Mod 1 **~4.9 mA** light sleep match measured values. Deep sleep **~1.4 mA** is ESP32 deep sleep + always-on domain (PSV2 off removes TCA4307 + photogate LED + other 3.3V2 loads).

### TCA4307DGKR — datasheet (§5.5 POWER SUPPLY, typ @ 25 °C)

Isolates main I2C from **SDA_2/SCL_2** (RTC on isolated segment). On **3.3V2** with PSV2.


| Parameter        | Symbol | Typ        | Max    | SleepModes mapping                             |
| ---------------- | ------ | ---------- | ------ | ---------------------------------------------- |
| Supply current   | `I_CC` | **2.5 mA** | 4.5 mA | EN enabled, bus idle (PSV2 on)                 |
| Shutdown current | `I_SD` | **10 µA**  | 30 µA  | EN = 0 (not used while RTC isolation required) |


### MAX98357A — datasheet (Electrical Characteristics, typ @ 25 °C)

On **3.3V2** with PSV2. `EXP_AMP_SD` (SD_MODE) via MCP23017: **LOW = shutdown**, **HIGH = channel select / active**.


| Parameter        | Symbol   | Typ        | Max    | SleepModes mapping                                      |
| ---------------- | -------- | ---------- | ------ | ------------------------------------------------------- |
| Quiescent current | `I_DD`  | **2.4 mA** | 2.85 mA | SD HIGH + I2S clocks (active — Demo/Speaker sketches)   |
| Standby current  | `I_STNDBY` | **340 µA** | 400 µA | SD HIGH, BCLK stopped (not used in SleepModes)          |
| Shutdown current | `I_SHDN` | **0.6 µA** | 2 µA   | **`EXP_AMP_SD` LOW** — light/deep prep in SleepModes    |
| Turn-on time     | `t_ON`   | **7 ms**   | 7.5 ms | After SD HIGH from shutdown (not ns — separate from I2S setup/hold) |


### GP1A57HR photointerrupter — datasheet (Electro-optical, typ @ 25 °C)

Sharp **GP1A57HR** on **3.3V2** (PSV2). IR LED tied **directly to 3.3V2** through **1 kΩ** series resistor (always on when PSV2 on). ESP32 GPIO (14/17/18/37) reads open-collector output only.


| Parameter | Symbol | Typ @ 5 V (datasheet) | On FED4 @ 3.3V2 (1 kΩ LED) | SleepModes mapping |
| --------- | ------ | --------------------- | --------------------------- | ------------------ |
| Forward current | `I_F` | 7 mA (test condition) | **~(3.3 − 1.1) / 1k ≈ 2.2 mA** | LED always on when PSV2 on |
| Supply current (output high) | `I_CCH` | 0.7 mA | **~0.3–0.5 mA** (scaled) | Detector IC, beam clear |
| Supply current (output low) | `I_CCL` | 1.7 mA | **~0.5–0.8 mA** (scaled) | Detector IC, beam blocked |
| **Per mounted gate** | — | ~8 mA @ 5 V / 7 mA `I_F` | **~2.5–3.0 mA** | **1× mounted on bench DUT** |

Four populated gates at the same bias → **~10–12 mA** additional PSV2 load. `PSV2_OFF()` in deep sleep removes photogate current entirely.

### Kyocera TN0216 LCD — datasheet (§6-1, 25 °C)

Panel logic on **3.3 V** (`V_DD` typ 3.3 V). Backlight (`EXP_DISPLAY_LED`) is separate MCP drive.


| Parameter         | Symbol     | Typ        | Max    | SleepModes mapping                                        |
| ----------------- | ---------- | ---------- | ------ | --------------------------------------------------------- |
| Operating current | `I_DD_opr` | **22 µA**  | 45 µA  | RST LOW — panel ON (light sleep, awake between refreshes) |
| Standby current   | `I_DD_stb` | **1.5 µA** | 5.5 µA | RST HIGH — panel blanked (deep sleep)                     |
| Input leak        | `I_IN`     | 5 nA       | 20 nA  | GPIO/SPI idle                                             |


**Implication:** Panel silicon is negligible (`I_DD` ≈ 22 µA). In **SleepModes** light sleep with **PSV2 on**, dominant loads are **TCA4307 ~2.5 mA (stock)** and **~2.5–3.0 mA per GP1A57HR** (1 kΩ LED on 3.3V2) — not the amp (`EXP_AMP_SD` LOW → ~0.6 µA). Deep sleep savings come from **`PSV2_OFF()`** removing isolator, photogate LED, and other 3.3V2 peripherals.

### Total current by mode (est.)


| Mode                                       | Low    | Typical      | High   | Dominant loads                                                                   |
| ------------------------------------------ | ------ | ------------ | ------ | -------------------------------------------------------------------------------- |
| **Awake** (active — SPI refresh, I2C init) | 45 mA  | **~65 mA**   | 90 mA  | ESP32-S3 CPU + SPI display; 3.3V2 idle bias                                      |
| **Light sleep** (5 s, VCOM keepalive)      | 3 mA   | **~5 mA**    | 8 mA   | ESP32 ~1.1 mA + touch; PSV2 on — **TCA4307 ~2.5 mA** (stock) + **GP1A57HR ~2.5–3 mA/gate** |
| **Deep sleep** (5 s, panel blanked)        | 0.1 mA | **~0.3 mA**  | 0.6 mA | ESP32 deep sleep; **PSV2 off** (panel `I_DD_stb` ≈ 1.5 µA)                       |
| **SleepModes avg**¹                        | 2 mA   | **~5–10 mA** | 15 mA  | ~50 % light / ~50 % deep + short awake bursts                                    |


¹ Time-weighted over one cycle (≈1 s awake @ 65 mA, 5 s light @ 5 mA, 5 s deep @ 0.3 mA) → **~7–9 mA** average pack draw. Dominated by light-sleep phase unless PSV2 is cut or display maintenance is reduced.

**Assumptions:** ESP32-S3 @ 240 MHz, WiFi/BT off, USB unplugged; **light sleep + touch ≈ 1.1 mA**. **TCA4307** `I_CC` ≈ **2.5 mA** when PSV2 on (stock). **GP1A57HR:** **~2.5–3.0 mA** per gate on 3.3V2 (1 kΩ LED, rail-tied). **MAX98357A:** ~**0.6 µA** in SleepModes (`EXP_AMP_SD` LOW). Kyocera `I_DD` ≈ 22 µA. Bench DUT: **1 photogate mounted**.

### SleepModes cycle (one iteration)


| Phase                            | Duration (nom.)        | ESP32 mode                                           |
| -------------------------------- | ---------------------- | ---------------------------------------------------- |
| Awake — draw status, sensor init | ~0.5–2 s (boot longer) | Active                                               |
| Light sleep — VCOM keepalive     | 5 s                    | `esp_light_sleep` (500 ms timer chunks + touch wake) |
| Awake — draw status              | ~0.1–0.5 s             | Active (SPI display refresh)                         |
| Deep sleep                       | 5 s                    | `esp_deep_sleep` (timer wake, reboot)                |


Effective duty cycle ≈ **50 % light sleep / 50 % deep sleep**, plus short active windows for display + I2C.

### Device states by phase


| Device / rail                          | Awake (active)                  | Light sleep                  | Deep sleep                | Est. (awake / light / deep)           |
| -------------------------------------- | ------------------------------- | ---------------------------- | ------------------------- | ------------------------------------- |
| **ESP32-S3**                           | Active (default CPU clk)        | Light sleep; touch FSM armed | Deep sleep                | 45–80 mA / **~1.1 mA** / 15–50 µA     |
| **3.3V2 `EXP_PSV2_EN`**                | ON (LOW)                        | ON                           | **OFF** (HIGH)            | —                                     |
| **3.3V3 `EXP_PSV3_EN`**                | ON at boot; OFF before sleep    | OFF                          | OFF                       | —                                     |
| **MCP23017T** (I2C `0x20`)             | I2C active                      | Idle; outputs LOW            | Idle; holds PSV2/PSV3 OFF | 0.3–1 mA / 1–50 µA / 1 µA             |
| **Kyocera MIP** (panel `I_DD` only)    | RST LOW; operating              | RST LOW; operating           | RST HIGH; standby         | 22 µA / 22 µA / 1.5 µA (typ)          |
| **LIS2DH12TR**                         | 50 Hz HR mode                   | Power-down                   | Power-down                | 0.1–0.2 mA / 2 µA / 2 µA              |
| **BME680**                             | Idle (heater off between reads) | Sleep                        | Sleep                     | 0.9 mA / 0.2 µA / 0.2 µA              |
| **VEML7700**                           | Enabled                         | `enable(false)`              | `enable(false)`           | 60–100 µA / 0.5 µA / 0.5 µA           |
| **VL53L1X**                            | `stopRanging()` after use       | `stopRanging()`              | `stopRanging()`           | 50 µA–1.4 mA² / 50–100 µA / 50–100 µA |
| **EKMB1107112 PIR**                    | GPIO input                      | GPIO input                   | GPIO input                | ~6 µA (all phases)                    |
| **TCA4307** I2C isolator (RTC segment) | EN on; PSV2 on                  | EN on; PSV2 on               | **PSV2 off**              | ~2.5 mA / ~2.5 mA / 0                 |
| **GP1A57HR photogate** (GPIO 14/17/18/37) | 3.3V2; LED on rail (1 kΩ)    | 3.3V2; **LED on** (PSV2 on)  | **PSV2 off** — LED off    | — / **~2.5–3 mA/gate** / 0            |
| **MAX98357A amp** (I2S, `EXP_AMP_SD`)  | PSV2 on; SD HIGH (audio)        | PSV2 on; **SD LOW** (shutdown) | **PSV2 off**              | ~2.4 mA / **~0.6 µA** / 0             |
| **DS3231 RTC**                         | On isolated I2C (stock)       | On isolated I2C (stock)      | **Unpowered** (stock)     | ~0.1–0.2 mA / ~0.1–0.2 mA / 0         |
| **3.3V2 domain** (SD, haptic, sol)     | Rail on; MCP outputs LOW        | Rail on                      | **Rail off**              | ~0.1 mA / ~0.1 mA / 0                 |
| **RGB strip + STATUS_LED**             | OFF                             | OFF                          | OFF                       | 0                                     |
| **Motor ULN2003LV**                    | Pins LOW                        | Pins LOW                     | Pins LOW                  | <10 µA                                |
| **MAX17048**                           | Active on VBATT                 | Active on VBATT              | Active on VBATT           | ~50 µA (all phases)                   |
| **→ Subtotal (est.)**                  | **~45–90 mA**                   | **~3–8 mA**                  | **~0.1–0.6 mA**           | **~45–90 mA / ~3–8 mA / ~0.1–0.6 mA** |


² VL53L1X draw highly state-dependent without XSHUT; confirm `stopRanging()` standby on hardware.

### Open power items (v1.7.1)


| Item                           | Notes                                                                                                                                                           |
| ------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Measured SleepModes totals** | Done — see **Measured (VBATT)** table above                                                                                                                     |
| **CPU 80 MHz**                 | `setCpuFrequencyMhz(80)` in awake windows only? Estimate savings vs display + I2C dominate                                                                      |
| **USB detection**              | No `USB.connected()` on ESP32 core 3.2.1; options: `Serial` DTR (bench only), VBUS GPIO if routed, or skip Serial wait on battery (current SleepModes approach) |
| **Display in deep sleep**      | Datasheet `I_DD_stb` ≈ 1.5 µA — measured 1.38 mA pack is almost entirely non-panel loads                                                                        |
| **PSV2 off during deep sleep** | Bench saw ~17 mA until PSV2 left on; suspect I2C/GPIO back-power into 3.3V2 — SleepModes quiesces PG + I2S + SDIO before rail off                               |
| **PSV2 on during light sleep** | **~2.5–3 mA/gate** GP1A57HR (1 kΩ LED on 3.3V2) + **~2.5 mA** TCA4307 (stock); Mod 1 removes isolator — gate LED still on while PSV2 on |
| **MCP23017T baseline**         | On always-on 3.3V in all phases — measure quiescent I2C expander draw; holds rail enables during deep sleep                                                     |


## v1.7 Power Rails

TPS22917 load switches: **LOW = rail ON**, **HIGH = rail OFF** (`EXP_PSV2_EN` = MCP pin 13, `EXP_PSV3_EN` = MCP pin 12).

### VBATT (LiPo)


| Peripheral                                | Notes                                |
| ----------------------------------------- | ------------------------------------ |
| ULN2003LV motor driver (GPIO 38/45/46/47) | VBATT; hold motor pins LOW when idle |
| MAX17048 fuel gauge (I2C `0x36`)          | VBATT; always powered on battery     |


### 3.3V (always on)


| Peripheral                                           | Notes                                                                                           |
| ---------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| MCP23017T GPIO expander (I2C `0x20`)                 | Always powered; drives PSV2/PSV3, display RST/LED, solenoids, haptic, amp SD — cannot rail-gate |
| VL53L1X ToF (I2C `0x29`)                             | Register shutdown for sleep                                                                     |
| VEML7700 lux (I2C `0x10`)                            | `enable(false)` for sleep                                                                       |
| EKMB1107112 PIR (GPIO 10)                            | Always powered; GPIO input                                                                      |
| BME680 (I2C `0x76`)                                  | Sleep mode + gas heater off                                                                     |
| LIS2DH12TR accel (I2C `0x19`)                        | Power-down data rate                                                                            |
| Kyocera MIP display (SPI, MCP RST/LED, GPIO VCOM/CS) | Always on 3.3 V; `I_DD_opr` 22 µA typ / `I_DD_stb` 1.5 µA typ (§6-1); VCOM toggle when ON       |


### 3.3V2 — `EXP_PSV2_EN`


| Peripheral                                 | Notes                                                                       |
| ------------------------------------------ | --------------------------------------------------------------------------- |
| DS3231 RTC (I2C `0x68`)                    | Rail-gated                                                                  |
| TCA4307DGKR I2C switch (SDA_2/SCL_2 → RTC) | Rail-gated; **`I_CC` typ 2.5 mA** when EN on (isolator always on with PSV2) |
| Haptic motor (`EXP_HAPTIC`)                | MCP + rail                                                                  |
| Speaker amplifier (`EXP_AMP_SD`)           | MCP SD + rail; **`I_SHDN` ~0.6 µA** (SD LOW) / **`I_Q` ~2.4 mA** (SD HIGH + I2S) |
| SD card (SPI CS GPIO 48)                   | Rail-gated                                                                  |
| Photogate GP1A57HR (GPIO 14/17/18/37)       | Rail-gated; **IR LED → 3.3V2 via 1 kΩ** (~2.2 mA) + detector (~0.3–0.8 mA) ≈ **2.5–3 mA/gate** |
| Solenoid driver (`EXP_SOL_1/2`)            | MCP + rail                                                                  |


### 3.3V3 — `EXP_PSV3_EN`


| Peripheral                                   | Notes      |
| -------------------------------------------- | ---------- |
| `RGB_STRIP` net and front RGB LEDs (GPIO 36) | Rail-gated |


---


| SKETCH                                     | PASS     | FUNCTIONS                                                                                                    | NOTES                                                                                                                                                                                                                                                                                                                                                                                         |
| ------------------------------------------ | -------- | ------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `FED4-Accel-Adafruit.ino`                  | ✓        | LIS3DH accelerometer, main I2C bus, serial accel readout                                                     |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Accel.ino`                           | deferred | LIS3DH accelerometer, MIP display, tilt-controlled ball demo                                                 | Duplicate functionality                                                                                                                                                                                                                                                                                                                                                                       |
| `FED4-Battery-Fuel-Guage.ino`              | ✓        | MAX17048 fuel gauge, main I2C bus, voltage/percent monitoring                                                | Passed; see note. Gauge on VBATT (not 3.3V). With USB and no LiPo, VBATT ~4.2V (MCP73831 + 100µF cap) — valid voltage/`isDeviceReady()`, invalid SOC, ~−4%/hr drain. USB removed: VBATT falls to ~1.5V then cap bleed. MCP73831 STAT pulsing = no pack. Misleading readings are a bench-only USB/no-pack artifact; off VBUS, normal pack operation should be fine. Confirm with real battery. |
| `FED4-Color-Chase-TouchPads-PhotoGate.ino` | deferred | Front RGB strip, touch pads, center photogate, I2S speaker, MCP power rails                                  | Duplicate functionality                                                                                                                                                                                                                                                                                                                                                                       |
| `FED4-SleepModes.ino`                      |          | Light/deep sleep alternation, VCOM keepalive, sensor shutdown, PSV2/PSV3 rails, touch wake                   | Requires USB CDC On Boot; replaces FED4-DeepSleep                                                                                                                                                                                                                                                                                                                                             |
| `FED4-VCOM-LEDC-Light-Sleep.ino`           | ✓        | Kyocera VCOM via LEDC PWM through `esp_light_sleep` (no CPU toggle chunks)                                   | Passed. Scope: ~502 ms period (~2 Hz) continues awake and in 10 s light sleep. Needs `LEDC_SLEEP_MODE_KEEP_ALIVE` + `LEDC_USE_RC_FAST_CLK` + `gpio_sleep_sel_dis` + `ESP_PD_DOMAIN_RC_FAST` ON. USB CDC On Boot.                                                                                                                                                                            |
| `FED4-Demo-Hardware.ino`                   | ✓        | Full hardware sweep: MIP dashboard, env/lux/tof/accel/RTC/battery, photogates, touch+strip, PIR INT, buttons | Requires USB CDC On Boot; motor excluded                                                                                                                                                                                                                                                                                                                                                      |
| `FED4-Display-Standalone.ino`              | ✓        | Kyocera MIP display, SPI, MCP reset/backlight, PSV2/PSV3 rails                                               |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Display.ino`                         | skipped  | Kyocera MIP display via FED4 library, SPI, MCP backlight toggle                                              | Refers to old hardware                                                                                                                                                                                                                                                                                                                                                                        |
| `FED4-Haptic.ino`                          | ✓        | Haptic motor, MCP23017, PSV2 power rail                                                                      |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-LED-Position-Test.ino`               | deferred | Front 8-LED strip index mapping, per-pixel colors, left/center/right groups                                  | Requires refactoring; duplicates FED4-LEDs-Front.ino                                                                                                                                                                                                                                                                                                                                          |
| `FED4-LEDs-Front.ino`                      | ✓        | Front RGB strip, MCP23017 PSV3, three buttons, LED animations                                                |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-LUX.ino`                             | ✓        | VEML7700 ambient light sensor, main I2C bus, lux readout                                                     |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Mario-SFX.ino`                       | deferred | I2S speaker via FED4 library, Mario-style sound effects                                                      | Duplicate functionality                                                                                                                                                                                                                                                                                                                                                                       |
| `FED4-Motor.ino`                           | ✓        | 4-wire stepper motor (pins 38/45/46/47), forward/reverse rotation                                            | See Stepper() pin order (IN1,IN3,IN2,IN4) and 2048 steps/rev for 28BYJ-48 — updated in sketch and library.                                                                                                                                                                                                                                                                                    |
| `FED4-PhotoGates.ino`                      | ✓        | Four photogate GPIO inputs (center, left, right, pellet detector)                                            | Partial: left/right nose poke untested; will mount test components to validate later.                                                                                                                                                                                                                                                                                                         |
| `FED4-PIR-Sensor.ino`                      | ✓        | PIR motion sensor GPIO input, status LED mirrors motion level                                                |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-RTC.ino`                             | ✓        | DS3231 RTC, main I2C via PSV2/TCA4307, date/time readout                                                     |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-SD-Card.ino`                         | ✓        | SD card over SPI, mount, directory listing, read/write                                                       |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Submodule-SeeedStudioSense.ino`      |            | TRRS TRIG+UART Sense master: senseSyncTime + senseTrigPulse                                             | Companion: `examples/3_Submodules/SeeedStudioSense/`; TRIG=TRRS2/GPIO5→Sense GPIO1, DATA=TRRS3/GPIO6→Sense GPIO2 |
| `FED4-Speaker-with-SD.ino`                 | deferred | I2S amplifier, SD card MP3 playback, buttons, MCP PSV2/amp enable                                            | Special functionality; requires MP3 file (largely software dev)                                                                                                                                                                                                                                                                                                                               |
| `FED4-Speaker.ino`                         | ✓        | I2S amplifier, MCP PSV2/amp enable, buttons, generated tones                                                 |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Solenoids.ino`                       | ✓        | Two solenoids on MCP23017 (EXP_SOL_1/2), alternating 500 ms                                                  |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-TRRS.ino`                            | ⚠        | TRRS audio jack inputs, front RGB strip group color feedback                                                 | Passed with critical fix: pins 2 and 5 need to be swapped; noted in schematic for v1.7.1.                                                                                                                                                                                                                                                                                                     |
| `FED4-Temp.ino`                            | ✓        | BME680 environmental sensor, main I2C bus, temp/humidity/pressure/gas                                        |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-ToF.ino`                             | ✓        | VL53L1X time-of-flight distance sensor, main I2C bus                                                         |                                                                                                                                                                                                                                                                                                                                                                                               |
| `FED4-Touch-Light-Sleep-Multiple.ino`      | deferred | Three touch pads, ESP32 light sleep with touchpad wake                                                       | Duplicate functionality                                                                                                                                                                                                                                                                                                                                                                       |
| `FED4-Touch-Pads-beeps.ino`                | deferred | Touch pads, I2S speaker beeps, status LED, MCP PSV2/PSV3                                                     | Duplicates core functionality                                                                                                                                                                                                                                                                                                                                                                 |
| `FED4-Touch.ino`                           | ✓        | Three ESP32 touch pads, status NeoPixel color feedback                                                       |                                                                                                                                                                                                                                                                                                                                                                                               |


