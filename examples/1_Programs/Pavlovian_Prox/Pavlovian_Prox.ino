/*
  Feeding experimentation device 4 (FED4)

  This task will play a 5s tone followed by a pellet dispense.  
  The mouse must remove the pellet to activate a variable 30-90s inter-trial-interval, afterwhich another tone will play

  *** WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories from 
  the task each day. For most mice this is ~150 20mg pellets (~3g of food) per 24 hours. *** 

*/

#include <FED4.h>           // include the FED4 library
FED4 fed4;                  // start FED4 object
char task[] = "Pavlovian";  // give the task a unique name
bool proxCompleted = false;

void setup() {
  fed4.begin(task);  // initialize FED4 hardware
  fed4.sleepSeconds = 1;
}

void loop() {
  Serial.println();
  Serial.println(fed4.prox());

  fed4.run();  // run this once per loop

  int proximity = fed4.prox();
  int pelletCount = fed4.pelletCount;

  if (
    (pelletCount < 50 && proximity >= 80 && proximity <= 140) || 
    (pelletCount < 100 && proximity >= 90 && proximity <= 130) || 
    (pelletCount < 150 && proximity >= 100 && proximity <= 120)) {
    proxCompleted = true;
  }

  if (proxCompleted == true) {
    // play Pavlovian tone and drop pellet
    fed4.logData("Tone");                        // log data for time of tone
    fed4.playTone(1000, 5000, 0.2);              // play 1000hz tone lasting 5000ms at 0.2 volume (volume goes from 0-1)
    fed4.feed();                                 // feed - the feed function will timeout after 20s or when  pellet is removed
    bool pelletPresent = fed4.checkForPellet();  // check for pellet

    while (pelletPresent) {                    // wait until pellet is removed
      pelletPresent = fed4.checkForPellet();   // check for pellet every 5s
      esp_sleep_enable_timer_wakeup(5000000);  // 5 second in microseconds
    }

    fed4.timeout(3, 6);  // variable timeout
    proxCompleted = false;
  }



  //also log pokes during the task
  if (fed4.leftTouch) {    // if poke is touched
    fed4.logData("Left");  // log data
  }

  if (fed4.centerTouch) {    // if poke is touched
    fed4.logData("Center");  // log data
  }

  if (fed4.rightTouch) {    // if poke is touched
    fed4.logData("Right");  // log data
  }
}
