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
  fed4.logProx = true;  //log prox sensor to SD card for 5s after a pellet is taken
  fed4.begin(task);  // initialize FED4 hardware
}

void loop() {
  fed4.run();  // run this once per loop

if (fed4.leftTouch) {     // if left poke is touched
    fed4.lowBeep();         // 500hz 200ms beep
    fed4.leftLight("red");  // light LEDs around left poke red
    fed4.logData("Left");

    // Check proximity sensor for 3s, log "Approach" if <20mm, "No_approach" otherwise
    unsigned long startTime = millis();
    while (millis() < startTime + 3000) {
        if (fed4.prox() < 20) {
            fed4.bopBeep();
            fed4.centerLight("white");
            fed4.logData("Approach");
            fed4.feed();
            return; // Exit early if approach detected
        }
        delay(10);
    }
    
    fed4.logData("No_approach");
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