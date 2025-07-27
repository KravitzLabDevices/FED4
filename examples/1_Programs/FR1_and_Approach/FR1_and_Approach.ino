/*
  Feeding experimentation device 4 (FED4)

  This modified "Fixed-Ratio 1" or "FR1" program will beep each time the mouse touches the Left poke. The mouse must then move in front of the pellet well 
  (<20mm as detected by the prox sensor) within 1s to activate the dispense, otherwise the trial will reset.
  Touching the center or right poke will result in a short click stimulus but nothing else will happen.
  
  This task is useful for quantifying simple learning rates and accuracy after acquisition.  
  Most mice can be trained on this task without any prior training, by running this task in their cage overnight for one night.

  *** WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories from 
  the task each day. For most mice this is ~150 20mg pellets (~3g of food) per 24 hours. *** 

*/

#include <FED4.h>              // include the FED4 library
FED4 fed4;                     // start FED4 object
char task[] = "FR1_Approach";  // give the task a unique name

void setup() {
  fed4.begin(task);  // initialize FED4 hardware
}

void loop() {
  fed4.run();  // run this once per loop

  if (fed4.leftTouch) {       // if left poke is touched
    fed4.lowBeep();           // 500hz 200ms beep
    fed4.centerLight("red");  // light LEDs around center poke red
    fed4.logData("Left");
    
    // Check proximity sensor for 1s, log "Approach" if <20mm, "No_approach" otherwise
    unsigned long startTime = millis();
    while (millis() < startTime + 1000) {
      int proximity = fed4.prox();
      if (proximity > 0 && proximity < 20) {
        fed4.bopBeep();
        fed4.centerLight("white");
        fed4.logData("Approach");
        fed4.feed();
        return;  // Exit early if approach detected
      }
      delay(10);
    }
    fed4.logData("No_approach");
  }

  if (fed4.centerTouch) {  // if center poke is touched
    fed4.click();          // audio click stimulus
    fed4.logData("Center");
  }

  if (fed4.rightTouch) {  // if right poke is touched
    fed4.click();         // audio click stimulus
    fed4.logData("Right");
  }
}