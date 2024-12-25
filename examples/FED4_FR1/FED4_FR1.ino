#include <FED4.h>               // include the FED4 library

FED4 fed4;                      // start FED4 object

void setup() {
  fed4.begin();                 // initialize FED4 hardware
}

void loop() {
  fed4.run();                   // run this once per loop
  
  if (fed4.leftTouch) {         // if left poke is touched
    // fed4.sound1();
    fed4.feed();                // feed one pellet
  }
}