#include <FED4.h>

FED4 fed;

void setup() {
  fed.begin();
}

void loop() {
  fed.run();  // run this once per loop
  
  if (fed.leftTouch) {    //if Left poke is touched:
    fed.feed();
  }
}