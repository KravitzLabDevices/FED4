# FED4 firmware flash tracker

Each in-lab flash of touch-test firmware gets a **4th-digit** version (`1.7.0.1`, `1.7.0.2`, …) so a unit’s screen, Serial boot line, and CSV `LibraryVer` column all identify what was loaded.

## Versioning rules

| Piece | Where | When to bump |
|-------|--------|----------------|
| **Board** | `FED4_BOARD_VERSION_STR` | Hardware revision (currently `1.7.0`) |
| **Arduino library package** | `library.properties` `version=` | Published library release (currently `1.7.0`) |
| **Flashed firmware** | `FED4_FIRMWARE_VERSION_STR` / `FED4::libraryVer` | **Only when `src/` library code changes** |

- Start this test campaign at **`v1.7.0.1`**.
- Sketch-only edits (task name, comments, analysis scripts) **do not** bump the firmware number.
- Library change → bump the 4th digit and add a page under this folder **before** flashing units.
- Display shows `v` + `libraryVer` on the Task row (right) and in the footer.

## How to tell which flash / arm is running

Look at the status screen after boot:

| Field | Meaning |
|-------|---------|
| **Task: AwakeDrift** | [`TouchDriftLog_Awake`](../../examples/3_Troubleshooting/TouchDriftLog_Awake/) — never sleeps |
| **Task: SleepDrift** | [`TouchDriftLog_Sleep`](../../examples/3_Troubleshooting/TouchDriftLog_Sleep/) — `waitUntil()` light sleep |
| **v1.7.0.1** (Task-right + footer) | Firmware identity for this library build |

CSV columns `LibraryVer` and `Program` record the same pair on every behavioral and `_T.CSV` row.

## Flash log

| Firmware | Library change? | Sketches | Status | Notes |
|----------|-----------------|----------|--------|-------|
| [v1.7.0.1](v1.7.0.1.md) | Yes — first tracked test flash of the touch-fix library | AwakeDrift + SleepDrift | Ready to flash | Touch FSM timeout / measurement / ID / drift-log campaign |
