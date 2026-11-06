/*
  Feeding Experimentation Device 4 (FED4)

  FR1 — one pellet for each left poke.
  Center and right pokes are logged and do not dispense.

  If a pellet remains after feed()'s ~20 s awake watch, a later poke logs
  LeftWithPellet / CenterWithPellet / RightWithPellet and feed() does not
  dispense again. LatePelletTaken is logged on a later wake once the well
  is empty (timer/touch/button) — coarse retrievalTime.
*/

#include <FED4.h>

FED4 fed4;

void setup() {
  fed4.begin("FR1");     // Start hardware, load the mouse ID, and open FED4_<id>_YYYYMMDD_NN.CSV.
}

void loop() {
  fed4.run();            // Refresh the screen and poll sensors, run once per loop

  if (fed4.leftTouch) {  // If left poke is detected
    fed4.feed();         // Dispense one pellet
  }
}