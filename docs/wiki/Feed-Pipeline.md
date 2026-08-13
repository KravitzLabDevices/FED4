# FED4 Feed Pipeline

How pellet delivery, awake retrieval timing, and **late retrieval** (after the 20 s window) work. Motor/jam hardware details: [Motor-and-Pellet-Dispensing.md](Motor-and-Pellet-Dispensing.md).

## Roles

| API | Owns |
|-----|------|
| **`feed()`** | Dispense (jam *clears* while motor still trying; **`DispenseError` only on hard give-up**), settle, well monitor **≤20 s** for **precise** `retrievalTime`, then `PelletTaken` **or** set **`pendingRetrieval`** if pellet still in well |
| **`waitUntil()` / sleep** | While `pendingRetrieval` and pellet present, arm **`PHOTOGATE_1` HIGH** light-sleep wake; on any wake call **`checkLateRetrieval()`** and log **`LatePelletTaken`** when the well is empty |

A pellet does **not** appear without the motor. Late handling is only for a pellet **already in the well** after the awake window. Sketches (e.g. BasicFED4) do **not** need to call `checkLateRetrieval()` themselves.

**BasicFED4** (poke → feed):

```cpp
FedEvent e = fed4.waitUntil();
if (e.source == FedWakeSource::Touch && e.pad == FedPad::Left) {
  fed4.feed();
  fed4.update();
}
```

**FreeFeeding** (replace when taken — do not re-dispense while well occupied):

```cpp
while (fed4.checkForPellet()) {
  fed4.waitUntil(); // photogate wake + LatePelletTaken when pending
}
fed4.feed();
fed4.update();
```

## Why 20 s awake, then photogate wake?

- **≤20 s in `feed()`:** stay awake and poll so `PelletTaken` retrieval times are accurate.
- **After 20 s still present:** light sleep; **well photogate** wakes when the beam clears (pellet taken) so logging is not stuck on the 60 s UI timer. `LatePelletTaken` timing may include a small wake skew vs continuous polling.

## Flowchart

```mermaid
flowchart TD
  init[initFeeding] --> dispense[dispense]
  dispense -->|jam clears motor still active| dispense
  dispense -->|hard give-up| jamEvt[log DispenseError]
  dispense -->|well or drop| dropEvt[log PelletDrop]
  dropEvt --> settle[settle up to 500ms]
  settle -->|never in well| notDet[terminal PelletNotDetected]
  settle -->|in well| well[monitor well up to 20s awake]
  well -->|well clears| taken[terminal PelletTaken precise time]
  well -->|poke while present| withPellet[log WithPellet keep retrievalTime]
  withPellet --> well
  well -->|still present at 20s| pending[pendingRetrieval]
  pending --> sleep[waitUntil light sleep]
  sleep -->|arm PHOTOGATE_1 HIGH| sleep
  sleep -->|photogate or other wake| check[checkLateRetrieval in waitUntil]
  check -->|well empty| lateTaken[LatePelletTaken]
  check -->|still full| sleep
```

## CSV events

| Event | When |
|-------|------|
| `PelletDrop` | Dispense succeeded (well and/or drop); count incremented |
| `LeftWithPellet` / `CenterWithPellet` / `RightWithPellet` | Poke while pellet in well (does not clear retrieval time) |
| `PelletTaken` | Well cleared **during** the awake ≤20 s window (precise `RetrievalTime`) |
| `LatePelletTaken` | Well cleared **after** that window (`checkLateRetrieval` after photogate/timer/touch wake) |
| `PelletNotDetected` | `PelletDrop` but well empty after settle — not a late-retrieval case |
| `DispenseError` | Hard jam give-up only (`jammed()`) — not during jam-clear moves |

ENV/battery on every row: last `update()` → `refreshSensors()` snapshot.

## Gaps identified → remedies

| Gap | Remedy |
|-----|--------|
| Late take only visible on 60 s `waitUntil` timer | Arm `PHOTOGATE_1` GPIO wake while pending; library calls `checkLateRetrieval()` in `waitUntil` |
| Sketch had to call `checkLateRetrieval()` | Moved into library sleep/wake path; BasicFED4 stays poke → feed → update |
| “Wait for pellet to appear” after settle miss | Removed — late = late **take** of pellet still in well |
| `DispenseError` during jam clears | Error only from `jammed()` |

## Notes

- Photogate wake is **HIGH-level** and enabled **only** when `pendingRetrieval && checkForPellet()` (empty well is already HIGH — would spam-wake if left armed).
- `FedWakeSource::Pellet` marks photogate-driven wakes so sketches do not treat them as button presses.
- `feed()` also calls `checkLateRetrieval()` at entry before a new dispense.
