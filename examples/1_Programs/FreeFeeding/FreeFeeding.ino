/*
  Feeding Experimentation Device 4 (FED4)

  FreeFeeding — dispense a pellet whenever the well is empty (replace when
  taken). Useful for quantifying total pellet intake.

  Light sleep is not required between pellets: feed() blocks while a pellet
  is in the well; VCOM stays alive via LEDC whenever the panel is on.
*/

#include <FED4.h>

FED4 fed4;

void setup() {
  Serial.begin(115200);
  fed4.begin("FreeFeeding");
}

void loop() {
  fed4.feed();    // dispense, wait until taken (or error/timeout)
  fed4.update();  // refresh counters / serial after each cycle
}
