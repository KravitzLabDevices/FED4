/*
  Feeding Experimentation Device 4 (FED4)

  BasicFED4 — left poke dispenses a pellet, then wait until the next poke.
  Light sleep keeps MIP VCOM alive; app wakes on touch/button or every 60 s
  for display/serial housekeeping (not every 4 s).
  STATUS_LED mirrors PIR when motion sensing is enabled (Demo-Hardware).
*/

#include <FED4.h>

FED4 fed4;

void setup() {
  Serial.begin(115200);
  fed4.useMotionSensor = true;  // before begin — GPIO + PIR→LED in begin/run
  fed4.begin("BasicFED4");
  fed4.serviceHousekeeping();   // show main screen before first sleep
}

void loop() {
  FedEvent e = fed4.waitUntil(60);  // touch / button / 60 s housekeeping

  // Refresh while *Touch flags are still set so poke dots show
  fed4.serviceHousekeeping();

  if (e.source == FedWakeSource::Touch && e.pad == 1) {
    fed4.feed();
    fed4.serviceHousekeeping();  // post-feed counters / pellet state
  }
}
