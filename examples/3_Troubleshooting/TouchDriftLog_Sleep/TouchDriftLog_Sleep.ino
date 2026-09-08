/*
  FED4 Touch Drift Log — LIGHT SLEEP arm (cage / production-shaped)

  Production-shaped BasicFED4 loop (waitUntil + left poke feeds) whose only
  addition is the touch diagnostic CSV. This is the arm that reproduces the
  field failure: pokes stop being detected after hours in a cage.

  Writes /FED4_<id>_<YYYYMMDD>_<NN>_T.CSV alongside the normal behavioral CSV:
    BootChar   one row per boot — the per-device, per-port characterization table
    Heartbeat  every timer wake (60 s) — idle sampled when nothing is happening
    Poke       resolved touch wake, with PeakSmooth + PokeDuration
    TouchMiss  touch wake that resolved to no pad (primary failure signature)
    Rechar     startSleep()'s 2 s rescue characterization fired

  REQUIRES FED4_ENABLE_TOUCH_LOG = 1 in src/FED4.h — it is a LIBRARY flag, so a
  #define here does NOT reach the library under the Arduino IDE. Rebuild after
  changing it.

  Display: Task field = "SleepDrift"; footer / Task-right = firmware v1.7.0.1
  (see docs/firmware/v1.7.0.1.md). Firmware number bumps only when src/ changes.

  Analyse with extras/analysis/fed4_touch_analysis.py.
*/

#include <FED4.h>

FED4 fed4;

void setup()
{
  // Set before begin() so the BootChar row is labelled correctly
  fed4.touchLogMode = "LightSleep";

  fed4.begin("SleepDrift");

#if !FED4_ENABLE_TOUCH_LOG
  Serial.println("WARNING: FED4_ENABLE_TOUCH_LOG is 0 — no touch log will be written.");
  Serial.println("         Set it to 1 in src/FED4.h and rebuild the library.");
#else
  if (!fed4.isTouchLogAvailable())
  {
    Serial.println("WARNING: touch log file was not created — check the SD card.");
  }
#endif
}

void loop()
{
  // 60 s heartbeat: the interval at which idle/benchmark drift is sampled
  FedEvent e = fed4.waitUntil(60);

  if (e.source == FedWakeSource::Touch && e.pad == FedPad::Left)
  {
    fed4.feed();
    fed4.update(); // post-feed counters / ENV / display
  }
}
