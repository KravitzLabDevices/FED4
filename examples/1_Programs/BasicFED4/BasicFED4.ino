/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep: PSV2 on; VCOM via LEDC KEEP_ALIVE (one long sleep).
  Poke logging enabled (FED4_DIAG_SKIP_SD_LOG=0).

  If a pellet remains after feed()'s ~20 s awake watch, a later poke logs
  LeftWithPellet / CenterWithPellet / RightWithPellet and feed() does not
  dispense again. LatePelletTaken is logged on a later wake once the well
  is empty (timer/touch/button) — coarse retrievalTime.
*/

#include <FED4.h>

FED4 fed4;

void setup()
{
  // Start hardware, load the mouse ID, and open FED4_<id>_YYYYMMDD_NN.CSV.
  fed4.begin("BasicFED4");
}

void loop()
{
  // Set by the previous wake. False on the first pass, and false for
  // center, right, button, and timer wakes.
  if (fed4.leftTouch)
  {
    // Dispense one pellet, then watch the well. If a pellet is already
    // there, feed() returns without dispensing.
    fed4.feed();
  }

  // Refresh the screen, then sleep until a poke, a button, or 60 seconds.
  fed4.run();
}
