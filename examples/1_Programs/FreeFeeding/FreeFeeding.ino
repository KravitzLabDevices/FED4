/*
  Feeding experimentation device 4 (FED4)

  This "FreeFeeding" program will dispense a pellet automatically
  and replace it each time that it is removed. 
  
  This is useful for quantifying total pellet intake.
*/

#include <FED4.h>               // include the FED4 library

FED4 fed4;                      // start FED4 object

// TODO: needs a method for updating the JSON "program" item

void setup() {
  fed4.begin();                 // initialize FED4 hardware
}

void loop() {
  fed4.run();                   // run this once per loop
  fed4.feed();                  // feed one pellet
}