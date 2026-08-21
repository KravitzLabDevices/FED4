/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep: PSV2 on; VCOM via LEDC KEEP_ALIVE (one long sleep).
  Poke logging enabled (FED4_DIAG_SKIP_SD_LOG=0).

  If a pellet remains after feed()'s ~20 s awake watch, waitUntil() logs
  LatePelletTaken on a later wake (timer/touch/button) — coarse retrievalTime.
*/

#include <FED4.h>

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
    fed4.feed();
    fed4.update(); // post-feed counters / ENV / display
  }
}
