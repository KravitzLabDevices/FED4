# FED4 Buttons Functionality

The FED4 has **three user buttons** (GPIO **BUTTON_1** = 40, **BUTTON_2** = 39, **BUTTON_3** = 14), arranged bottom / middle / top. They use internal pull-down and can **wake the device** from light sleep. There is **no general-purpose “button pressed” API** for sketches; the library assigns fixed behaviors.

**Default behavior (on wake)**

- **`checkButton1()`** — **Button 1 (bottom):** Hold ~1 s → test pellet dispense (`feed()`), `bopBeep()`.
- **`checkButton2()`** — **Button 2 (middle):** Hold ~1 s → **device reset** (red wipe, `resetJingle()`, `esp_restart()`).
- **`checkButton3()`** — **Button 3 (top):** Hold **500 ms** → **audio toggle** (`silence` / `unsilence`), plus `click()`. Hold **1500 ms** → **menu** (`menu()`), `hapticDoubleBuzz()`.

These are run from **`wakeUp()`** after a **button** wake (not touch). Poll with **`digitalRead(BUTTON_1)`** etc. (HIGH = pressed) if you need raw state elsewhere (e.g. menu, dispense loop).

See [FED4_Buttons.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Buttons.cpp), [FED4_Pins.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Pins.h).
