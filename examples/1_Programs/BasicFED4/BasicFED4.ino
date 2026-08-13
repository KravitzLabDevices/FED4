/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep keeps MIP VCOM alive via LEDC; app wakes on touch/button or
  every 60 s for a display/serial refresh.
*/

#include <FED4.h>

FED4 fed4;

void setup()
{
  Serial.begin(115200);
  fed4.begin("BasicFED4");
}

void loop()
{
  FedEvent e = fed4.waitUntil(); // default 60 s UI refresh

  if (e.source == FedWakeSource::Touch && e.pad == FedPad::Left)
  {
    fed4.feed();
    fed4.update(); // post-feed counters / pellet state
  }
}
