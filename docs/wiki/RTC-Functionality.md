# FED4 RTC (Real-Time Clock) Functionality

The FED4 uses a **DS3231 RTC** on the secondary I2C bus to keep accurate time across power cycles. Time is used for **logs, display, and scheduling**.

**Initialization and auto-update**

- **`initializeRTC()`** — starts the DS3231 and loads time into the internal clock. On **new compilation**, the RTC is updated to the build time.
- **Compilation detection** — a stored compile timestamp in Preferences triggers **`updateRTC()`** only when firmware changes.

**Key API**

- **`now()`** — returns `DateTime` from the RTC.
- **`updateTime()`** — updates `currentHour`, `currentMinute`, `currentSecond`, `unixtime`.
- **`adjustRTC(timestamp)`** — sets RTC from a Unix timestamp (used by Hublink time sync).

**Logging**

- **`ElapsedSeconds`** in the SD log is **millis-based**, rounded to **0.001 s** (1 ms resolution).

**Manual set**

- **`menuRTC()`** — interactive RTC setting in the FED4 menu.

See [FED4_RTC.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_RTC.cpp), [RTClib](https://github.com/adafruit/RTClib). 
