/*
  Feeding experimentation device 4 (FED4)

  This "Fixed-Ratio 1" or "FR1" program will dispense a pellet 
  each time the mouse touches the Left poke. 
  
  This is useful for quantifying simple learning rates and 
  accuracy.
*/

#include <FED4.h>  // include the FED4 library

FED4 fed4;  // start FED4 object

// TODO: needs a method for updating the JSON "program" item
// TODO: add conditioned stimuli and poke logging

void setup() {
  fed4.begin();               // initialize FED4 hardware
}

void loop() {
  fed4.run();                 // run this once per loop

  if (fed4.leftTouch) {       // if left poke is touched
    fed4.feed();              // feed one pellet
  }

  if (fed4.rightTouch) {     // if right poke is touched
    fed4.haptic();       // buzz, default is 1 buzz for 200ms
  }
}