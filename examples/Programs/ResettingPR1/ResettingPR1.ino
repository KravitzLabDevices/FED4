/*
  Feeding experimentation device 4 (FED4)

  This is a resetting progressive ratio task meant to be used in a closed economy 
  setting as in Mourra et al., 2020: https://pubmed.ncbi.nlm.nih.gov/31809732/
  
  The task starts by requiring one poke on the Left (ie: FR1) to dispense a pellet.
  After the first poke the requirement incrememnts by 1 for each earned pellet (ie: FR1, FR3, FR5, etc.)
  If 30 minutes pass without a left or right poke the requirement resets to FR1 and the game starts again.
  
  This is useful for quantifying the "thriftiness" in their strategy for getting pellets.

*/

#include <FED4.h>                                          // include the FED4 library

FED4 fed4;                                                 // start FED4 object

//store local variables for this task
int poke_num = 0;                                          // umber of pokes since last pellet
int pokes_required = 1;                                    // current FR
unsigned long poketime = 0;                                // time of poke
int resetInterval = 30;                                    // number of minutes without a poke before requirement resets to FR1

// TODO: needs a method for updating the JSON "program" item

void setup() {
  fed4.begin();                                            // initialize FED4 hardware
}

void loop() {
  fed4.run();                                              // run this once per loop
  checkReset();                                            // Check if it's time to reset to FR1
  if (fed4.leftTouch) {                                    // if left poke is touched
    fed4.leftLight("red");                                 // light LEDs around left poke red
    fed4.click();                                          // auditory click stimulus
    poke_num++;                                            // increment poke number.
    poketime = fed4.unixtime;                              // update the current time of poke
    if (poke_num == pokes_required) {                      // check if current FR has been achieved
      fed4.lowBeep();                                      // 500hz 200ms beep
      fed4.feed();                                         // Deliver pellet
      fed4.outputPulse(2, 300);                            // Give an output line and duration (output 2 works for a mono 3.5mm output connector)
      pokes_required += 1;                                 // Edit this line to change the PR incremementing formula.  Default is for each pellet add 1 to the pokes required.
      poke_num = 0;                                        // reset poke_num to 0
    }
  }

  if (fed4.centerTouch) {                                  // if center poke is touched
    fed4.centerLight("green");                             // light LEDs around center poke green
    fed4.click();                                          // auditory click stimulus
  }

  if (fed4.rightTouch) {                                   // if right poke is touched
    fed4.rightLight("blue");                               // light LEDs around right poke blue
    fed4.click();                                          // auditory click stimulus
  }
}

// check if resetInterval has elapsed since last poke
void checkReset() {
  if (fed4.unixtime - poketime >= (resetInterval * 60)) {  // if the reset interval has elapsed since last poke
    poke_num = 0;                                          // set the poke_num back to 0
    pokes_required = 1;                                    // set the ratio back to FR1
    poketime = fed4.unixtime;                              // update the time of the last poke
  }
}
