# FED4 Poke (Nose-Poke) Functionality

The FED4 has **three nose-pokes** (left, center, right) detected by **ESP32 capacitive touch** pads (see [FED4_Pins.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Pins.h)). **Touch wakes** the device from light sleep.

**Program path**

- Prefer **`waitUntil()`** — on poke wake the library runs **`capturePoke()`** (rise-fraction identify, hold → `pokeDuration`, counters/flags), then **`logData("Left"|"Center"|"Right")`**, then **`update()`**.
- Sketches act on the returned **`FedEvent`** (`source` / `pad`); they do **not** need to call `logData` for the poke itself.
- CSV ENV/battery columns on every row are the last **`update()` → `refreshSensors()`** snapshot (not re-polled per event).
- During **`feed()`** while a pellet is in the well, additional pokes log as **`LeftWithPellet`** / **`CenterWithPellet`** / **`RightWithPellet`** (separate from the wake poke row).

**Members:** `leftTouch` / `centerTouch` / `rightTouch`, counters `leftCount` / `centerCount` / `rightCount`, `pokeDuration` (ms). Calibration runs at startup and periodically after feeds.

See [FED4_Touch.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Touch.cpp), [FED4_Sleep.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Sleep.cpp).
