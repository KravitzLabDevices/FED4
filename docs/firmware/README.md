# FED4 firmware flash tracker

Each flashed library build is identified by `FED4_FIRMWARE_VERSION_STR` so a unit’s screen, Serial boot line, and CSV `LibraryVer` column all name what was loaded.

## Versioning rules

| Piece | Where | When to bump |
|-------|--------|----------------|
| **Board** | `FED4_BOARD_VERSION_STR` | Hardware revision (currently `1.7.0`) |
| **Arduino library package** | `library.properties` `version=` | Published library release (currently `1.7.1`) |
| **Flashed firmware** | `FED4_FIRMWARE_VERSION_STR` / `FED4::libraryVer` | Same string as the library package. Bump when `src/` changes. |

- The 4th-digit lab campaign (`1.7.0.1`) was for in-lab touch-fix testing only. Published library is now **`1.7.1`**. Do not invent `1.7.0.2`.
- Display / Serial / CSV `LibraryVer` all come from `FED4_FIRMWARE_VERSION_STR` — do not hardcode a second string.
- Sketch-only edits (task name, comments, analysis scripts) **do not** bump the firmware number.
- Library change → bump `FED4_FIRMWARE_VERSION_STR` and `library.properties` together, and add a page under this folder **before** flashing units.
- Display shows `v` + `libraryVer` in the status footer (and RTC menu footer).

## How to tell which flash / arm is running

Look at the status screen after boot:

| Field | Meaning |
|-------|---------|
| **Task: AwakeDrift** | [`TouchDriftLog_Awake`](../../examples/3_Troubleshooting/TouchDriftLog_Awake/) — never sleeps |
| **Task: SleepDrift** | [`TouchDriftLog_Sleep`](../../examples/3_Troubleshooting/TouchDriftLog_Sleep/) — `waitUntil()` light sleep |
| **v1.7.1** (status footer) | Firmware identity for this library build |

CSV columns `LibraryVer` and `Program` record the same pair on every behavioral and `_T.CSV` row.

## Flash log

| Firmware | Library change? | Sketches | Status | Notes |
|----------|-----------------|----------|--------|-------|
| [v1.7.1](v1.7.1.md) | Yes — published library; keeps the validated 3.2.1 touch fixes | SleepDrift / BasicFED4 (production); AwakeDrift + SleepDrift (diag, log flag =1) | Ready to flash | 2 s rechar gated by HW-inactive + wakePad; no CalReject / bench watchdog; touch log and poke-timing off by default |
