/*
  Feeding Experimentation Device 4 (FED4)

  FreeFeeding — keep a pellet available: when the well is empty, dispense;
  when a pellet is present, wait until it is taken, then replace.

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
  // Do not start a new dispense while a pellet is still in the well
  while (fed4.checkForPellet())
  {
    fed4.waitUntil(); // timer/touch/button; LatePelletTaken when pending + empty
  }

  fed4.feed();   // dispense + awake retrieval window (or settle error)
  fed4.update(); // counters / ENV / display after each delivery cycle
}
