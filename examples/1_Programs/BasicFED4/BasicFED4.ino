/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep: PSV2 on; VCOM via LEDC KEEP_ALIVE (one long sleep).
  Poke logging enabled (FED4_DIAG_SKIP_SD_LOG=0).

  If a pellet remains after feed()'s ~20 s awake watch, waitUntil() logs
  LatePelletTaken on a later wake (timer/touch/button) — coarse retrievalTime.

  Optional SeeedStudio Sense: set FED4_ENABLE_SUBMODULE to 1 in src/FED4.h,
  flash examples/3_Submodules/SeeedStudioSense/, then ENABLE_SEEED_SENSE below.
*/

#include <FED4.h>

// #define ENABLE_SEEED_SENSE

FED4 fed4;

void setup()
{
  fed4.begin("BasicFED4");
}

void loop()
{
  FedEvent e = fed4.waitUntil(); // default 60 s UI refresh

  if (e.source == FedWakeSource::Touch && e.pad == FedPad::Left)
  {
#if defined(ENABLE_SEEED_SENSE) && FED4_ENABLE_SUBMODULE
    // Awake only — sync RTC + CAPTURE (returns after ACK; Sense finishes alone)
    fed4.senseSyncAndCapture();
#endif
    fed4.feed();
    fed4.update(); // post-feed counters / ENV / display
  }
}
