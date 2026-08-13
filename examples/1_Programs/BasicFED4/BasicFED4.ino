/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep keeps MIP VCOM alive via LEDC (PSV2 off for battery life).

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
