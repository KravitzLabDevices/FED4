# FED4 Audio Functionality

The FED4 uses an **I2S-driven speaker** (MAX98357A). Audio is initialized in `begin()`.

**Tones and beeps**

- **`playTone(frequency, duration_ms, amplitude)`** — single tone (e.g. `playTone(500, 200, 0.25)`).
- **`lowBeep()`** — 500 Hz, 200 ms. **`highBeep()`** — 1000 Hz. **`higherBeep()`** — 2000 Hz.
- **`click()`** — short click (quick feedback).
- **`bopBeep()`** — two-tone beep (1000 Hz → 1600 Hz).

**Jingles**

- **`playStartup()`** — startup melody. **`resetJingle()`** — power-cycle jingle. **`menuJingle()`** — menu arpeggio.

**Other**

- **`soundSweep(startFreq, endFreq, duration_ms)`** — frequency sweep. **`noise(duration_ms, amplitude)`** — white noise.
- **`silence()`** / **`unsilence()`** — mute audio (persists across reboots). **`enableAmp(bool)`** — amp on/off. **`resetSpeaker()`** — reinit I2S.

See [FED4_Audio.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Audio.cpp).
