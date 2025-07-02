/*
  Feeding experimentation device 4 (FED4)

  This "Fixed-Ratio 1" or "FR1" program will beep and dispense a pellet 
  each time the mouse touches the Left poke. Touching the center or right
  poke will result in a short click stimulus.
  
  This task is useful for quantifying simple learning rates and 
  accuracy after acquisition.  Most mice can be trained on this task without
  any prior training, by running this task in their cage overnight for one night.

  *** WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories from 
  the task each day. For most mice this is ~150 20mg pellets (~3g of food) per 24 hours. *** 

*/

#include <FED4.h>          // include the FED4 library
FED4 fed4;                 // start FED4 object
char task[] = "FR1_Prox";  // give the task a unique name

void setup() {
  fed4.begin(task);  // initialize FED4 hardware
}

void loop() {
  fed4.run();  // run this once per loop

  if (fed4.leftTouch) {     // if left poke is touched
    fed4.lowBeep();         // 500hz 200ms beep
    fed4.leftLight("red");  // light LEDs around left poke red
    fed4.logData("Left");

    //Check proximity sensor to see if mouse is in front of pellet well within 3s
    unsigned long starttime = millis();
    int proximity = fed4.prox();  // Take Prox measurement
    while (millis() < starttime + 3000) {
      proximity = fed4.prox();  // Take Prox measurement
      if (proximity < 20) {
        fed4.bopBeep();             // two beeps
        fed4.centerLight("white");  // light LEDs around center poke
        fed4.logData("Approach");
        fed4.feed();  // feed one pellet, logging drop and retrieval
      } else {
        fed4.logData("No_approach");
      }
    }
  }

  if (fed4.centerTouch) {  // if center poke is touched
    fed4.click();          // audio click stimulus
    fed4.hapticBuzz();
    fed4.centerLight("green");  // light LEDs around center poke green
    fed4.logData("Center");
  }

  if (fed4.rightTouch) {      // if right poke is touched
    fed4.click();             // audio click stimulus
    fed4.rightLight("blue");  // light LEDs around right poke blue
    fed4.logData("Right");
  }
}