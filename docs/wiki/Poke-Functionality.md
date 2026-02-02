# FED4 Poke (Nose-Poke) Functionality

The FED4 has **three nose-pokes** (left, center, right) detected by **ESP32 capacitive touch** pads (see [FED4_Pins.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Pins.h)). **Touch wakes** the device from light sleep. After `run()` returns, check `leftTouch`, `centerTouch`, or `rightTouch`; then `logData("Left")` (etc.), `feed()`, and light/sound as needed. Counters: `leftCount`, `centerCount`, `rightCount`; hold duration: `pokeDuration` (ms). Calibration is automatic at startup and periodically; sensitivity can differ on USB vs battery.
