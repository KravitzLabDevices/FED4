/*
  Feeding experimentation device 4 (FED4)

  This "Sequence Training" program implements a sequence training task
  where the mouse must complete a specified sequence to get a reward.
  
  The sequence starts simple (requiring one poke) and adds an additional poke every N pellets.
  
  This task is useful for training mice to learn complex behavioral sequences
  and can be used to study sequence learning and memory.

  *** WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories from 
  the task each day. For most mice this is ~150 20mg pellets (~3g of food) per 24 hours. *** 

*/

#include <FED4.h>                      // include the FED4 library
#include "SequenceLearning_Helpers.h"  // include helper functions

FED4 fed4;                         // start FED4 object
char task[] = "SequenceLearning";  // give the task a unique name

// ============================================================================
// CONFIGURATION SETTINGS - MODIFY THESE TO CUSTOMIZE YOUR EXPERIMENT
// ============================================================================

// Core sequence settings
const char* targetSequence = "L,C,R,C,L,C,R,C";  // Target sequence for the mouse to learn
const int pelletsPerLevel = 50;                  // Pellets needed to advance to next level
const int sequenceTimeout = 10;                  // Timeout in seconds before the sequence is reset
const int startLevel = 1;                        // Level to start at (1 = first item, 3 = first 3 items, etc.)

// Sequence manager instance using core settings
SequenceManager sequenceManager(fed4, targetSequence, pelletsPerLevel, sequenceTimeout, startLevel);


void setup() {
  fed4.begin(task);  // initialize FED4 hardware
  fed4.logData("Sequence: " + sequenceManager.getCleanTargetSequence());
}

void loop() {
  fed4.run();  // run this once per loop

  // Turn on side red LED when USB is connected
  bool usbConnected = fed4.isUSBPowered();
  if (usbConnected) {
    fed4.redPix(1);    // Turn on side red LED 
  } else {
    fed4.noPix();      // Turn off side LED when USB disconnected
  }

  // Check for timeout
  sequenceManager.checkTimeout();

  //Check for button 1 press to advance sequence level
  if (digitalRead(BUTTON_1) == HIGH) {    
    sequenceManager.forceLevelAdvance();
    fed4.playTone(800, 100, 0.3);  // Audio feedback
    delay(50);
    fed4.playTone(1000, 100, 0.3);
    Serial.print("Button 1 pressed! Level advanced to: ");
    Serial.println(sequenceManager.getCurrentLevel());
    delay (1000);
  }

  if (fed4.leftTouch) {     // if left poke is touched
    fed4.leftLight("red");  // light LEDs around left poke
    sequenceManager.addToSequence("Left");
  }

  if (fed4.centerTouch) {      // if center poke is touched
    fed4.centerLight("blue");  // light LEDs around center poke
    sequenceManager.addToSequence("Center");
  }

  if (fed4.rightTouch) {       // if right poke is touched
    fed4.rightLight("green");  // light LEDs around right poke
    sequenceManager.addToSequence("Right");
  }
}