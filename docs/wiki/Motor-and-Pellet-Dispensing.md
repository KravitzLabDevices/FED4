# FED4 Motor and Pellet Dispensing

The FED4 drives a **stepper motor** to rotate a hopper and dispense pellets. The **dispenser is greatly improved over FED3**, with better jam handling, pellet-in-well detection, optional drop sensor, and retrieval/poke logging.

**Motor**

- **Stepper:** 4-phase, 512 steps/rev. Pins **MOTOR_PIN_1–4** (46, 37, 21, 38); **MOTOR_SPEED** = 24. **Battery required** for motor operation.
- **`initializeMotor()`** — configure pins, set speed (called from `begin()`).
- **`releaseMotor()`** — coil pins LOW to save power between moves.
- **`motorTurns`** — counts small steps during dispense; ~**25** ≈ one pellet position, ~**1000** ≈ one hopper rotation. Logged (as `motorTurns/25`) on **PelletTaken** etc.

**Dispensing and feeding**

- **`feed()`** — full sequence: **`initFeeding()`** → **`dispense()`** → **`handlePelletSettling()`** → **`handlePelletInWell()`** → **`finishFeeding()`**.
- **`dispense()`** — steps motor (`stepper.step(-10)`) until **`checkForPellet()`** (well) or **`didPelletDrop()`** (optional drop sensor). Every 25 steps, **`releaseMotor()`** + 1 s delay. **`handleJams()`** runs each iteration.
- **`checkForPellet()`** — pellet in well (photogate **EXP_PHOTOGATE_1**). **`didPelletDrop()`** — pellet dropped (optional **EXP_PHOTOGATE_4**); **`initializeDropSensor()`** probes for drop sensor at init.
- **`handlePelletSettling()`** — wait up to **500 ms** for pellet in well; **`dispenseError`** = true if not seen. Then **`handlePelletInWell()`** polls until pellet taken or **20 s** timeout (retrieval time, pokes, etc.).

**Jam handling**

- **`minorJamClear()`** — 200 steps, 1 s pause. **`majorJamClear()`** — 1000 steps. **`vibrateJamClear()`** — short back‑and‑forth wobble. Used at **100** and **200** motorTurns during dispense.
- **`jammed()`** — after **2000** motorTurns without dispense: **DispenseError** on display, **`logData("DispenseError")`**, motor off, then sleep loop; **Button 2** reset to recover.

**Relevant members:** **`pelletCount`**, **`pelletPresent`**, **`pelletDropped`**, **`pelletReady`**, **`dispenseError`**, **`retrievalTime`**, **`pelletDropTime`**, **`pelletWellTime`**, **`blockPelletCount`** / **`blockPokeCount`**.

See [FED4_Motor.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Motor.cpp), [FED4_Feed.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Feed.cpp), [FED4_Pins.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Pins.h).
