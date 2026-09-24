/*
  Feeding Experimentation Device 4 (FED4)

  FreeFeeding — keep a pellet available: dispense when the well is empty;
  when a pellet is present, waitUntil() until it is taken, then replace.

  Same loop shape as BasicFED4 (idle via waitUntil, then act), but the
  free-feed state machine keys off well occupancy instead of a left poke.

  feed() watches the well awake for ~20 s (precise retrieval). If still present,
  waitUntil() light-sleeps with PSV2 off; LatePelletTaken is logged on the next
  wake when the well is empty (coarse time, up to the UI interval).
*/

#include <FED4.h>

FED4 fed4;

void setup()
{
  fed4.begin("FreeFeeding");
}

void loop()
{
  if (fed4.checkForPellet())
  {
    // Stocked — sleep until taken / timer / button (LatePelletTaken on empty wake)
    fed4.waitUntil(); // default 60 s UI refresh
  }
  else
  {
    fed4.feed();
    fed4.update(); // post-feed counters / ENV / display
  }
}
