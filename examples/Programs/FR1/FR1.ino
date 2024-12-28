/*
  Feeding experimentation device 4 (FED4)

  This "Fixed-Ratio 1" or "FR1" program will beep and dispense a pellet 
  each time the mouse touches the Left poke. Touching the center or right
  poke will result in a short click stimulus.
  
  This task is useful for quantifying simple learning rates and 
  accuracy after acquisition.  Most mice can be trained on this task without
  any prior training, by running this task in their cage overnight for one night.

  WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories
  from the task each day.   For most mice this is ~150 20mg pellets per 24 hours.  
*/

#include <FED4.h>                           // include the FED4 library

FED4 fed4;                                  // start FED4 object

// TODO: needs a method for updating the JSON "program" item
// TODO: add logging

void setup() {
  fed4.begin();                             // initialize FED4 hardware
}

void loop() {
  fed4.run();                               // run this once per loop

  if (fed4.leftTouch) {                     // if left poke is touched
    fed4.lowBeep();      
    fed4.feed();                            // feed one pellet
  }

  if (fed4.centerTouch) {                   // if center poke is touched
    fed4.click();          
  }

  if (fed4.rightTouch) {                    // if right poke is touched
    fed4.click();          
  }
}