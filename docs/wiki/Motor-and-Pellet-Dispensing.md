# FED4 Motor and Pellet Dispensing

The FED4 drives a **stepper motor** to rotate a hopper and dispense pellets. For the full **feed → settle → well monitor → late pellet** event pipeline (flowchart, CSV events, gaps/remedies), see **[Feed-Pipeline.md](Feed-Pipeline.md)**.

**Motor**

- **Stepper:** 4-phase, 512 steps/rev. Pins **MOTOR_PIN_1–4** (46, 37, 21, 38); **MOTOR_SPEED** = 24. **Battery required** for motor operation.
- **`initializeMotor()`** — configure pins, set speed (called from `begin()`).
- **`releaseMotor()`** — coil pins LOW to save power between moves.
- **`motorTurns`** — counts small steps during dispense; ~**25** ≈ one pellet position, ~**1000** ≈ one hopper rotation. Logged (as `motorTurns/25`) on terminal feed events.

**Jam handling**

- **`minorJamClear()`** — 200 steps, 1 s pause (at ~100 motorTurns). **`vibrateJamClear()`** — short back‑and‑forth wobble (at ~200 motorTurns).
- **`jammed()`** — after **2000** motorTurns without dispense: **only then** **`logData("DispenseError")`** (hard give-up). Jam *clears* at 100/200 turns do **not** log an error while the motor is still trying.

**Sensors:** **`checkForPellet()`** — well photogate **PHOTOGATE_1**. **`didPelletDrop()`** — optional drop sensor **PHOTOGATE_4**.

See [FED4_Motor.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Motor.cpp), [FED4_Feed.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Feed.cpp), [FED4_Pins.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Pins.h).
