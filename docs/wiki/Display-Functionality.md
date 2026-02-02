# FED4 Display (Screen) Functionality

The FED4 uses a **Sharp Memory Display** (SPI, **DISPLAY_CS** = 17), **144×168** pixels, 1-bit (black/white). The FED4 class extends **Adafruit_GFX**, so you can use standard GFX calls (`drawPixel`, `fillRect`, `print`, `drawLine`, etc.) plus FED4-specific helpers.

**Initialization and refresh**

- **`initializeDisplay()`** — init display, allocate buffer, clear (called from `begin()`).
- **`updateDisplay()`** — redraw status (task, mouse ID, env, battery, SD, counters, indicators, date/time). Called from `run()` and elsewhere.
- **`refresh()`** — push the backing buffer to the display. Call after drawing.
- **`clearDisplay()`** — clear buffer (white) and refresh.

**Status helpers (used by `updateDisplay`)**

- **`displayTask()`**, **`displayMouseId()`**, **`displayEnvironmental()`**, **`displayBattery()`**, **`displaySDCardStatus()`**, **`displayCounters()`**, **`displayIndicators()`**, **`displayDateTime()`** — status layout.
- **`displayActivityMonitor()`** / **`displayActivityCounters()`** — when program is ActivityMonitor.
- **`displayInitStatus(msg)`** — show init message during `begin()`.
- **`displayLowBatteryWarning()`** — low-battery full-screen message.

**Constants:** `DISPLAY_WIDTH` = 144, `DISPLAY_HEIGHT` = 168; **`DISPLAY_BLACK`**, **`DISPLAY_WHITE`**, **`DISPLAY_INVERSE`**, **`DISPLAY_NORMAL`**.

See [FED4_Display.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Display.cpp), [FED4.h](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4.h). [Adafruit Sharp Memory Display](https://learn.adafruit.com/adafruit-sharp-memory-display-breakout), [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library).
