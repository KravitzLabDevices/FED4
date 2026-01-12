/*
  Feeding experimentation device 4 (FED4)

  This "Sequence Training" program implements a sequence training task
  where the mouse must complete a specified sequence to get a reward.
  
  The sequence starts simple (requiring one poke) and adds an additional poke every N pellets.
  
  This task is useful for training mice to learn complex behavioral sequences
  and can be used to study sequence learning and memory.

  LIGHT CUEING SYSTEM:
  - The next poke in the sequence is indicated by a yellow light
  - After each correct poke, the light moves to indicate the next poke
  - If the mouse makes an error, the sequence resets and the first poke is cued again
  - Light cues can be enabled/disabled using the lightCues flag in the configuration section

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
const char* targetSequence = "LCRCLCRC";  // Target sequence for the mouse to learn
const bool lightCues = true;                     // Set to true to enable light cues, false to disable
const int pelletsPerLevel = 30;                  // Pellets needed to advance to next level
const int sequenceTimeout = 30;                  // Timeout in seconds before the sequence is reset
const int startLevel = 1;                        // Level to start at (1 = first item, 3 = first 3 items, etc.)

// Sequence manager instance using core settings
SequenceManager sequenceManager(fed4, targetSequence, pelletsPerLevel, sequenceTimeout, startLevel, lightCues);

void setup() {
  // Enable Hublink functionality
  fed4.useHublink = false;
  
  // Initialize FED4 hardware
  fed4.begin(task);
  fed4.logData("Sequence: " + sequenceManager.getCleanTargetSequence());
  fed4.sleepyLEDs = false;
  
  // Show initial cue for the first poke
  if (lightCues) {
    sequenceManager.cueNextPoke();
  }
}

void loop() {
  fed4.run();  // run this once per loop

  //Check for button 1 press to advance sequence level
  if (digitalRead(BUTTON_1) == HIGH) {
    fed4.highBeep();
    sequenceManager.sequenceAdvance();
    delay(1000);
  }

  // Check for timeout
  sequenceManager.checkTimeout();

  if (fed4.leftTouch) {        // if left poke is touched
    sequenceManager.addToSequence("Left");
  }

  if (fed4.centerTouch) {      // if center poke is touched
    sequenceManager.addToSequence("Center");
  }

  if (fed4.rightTouch) {       // if right poke is touched
    sequenceManager.addToSequence("Right");
  }
}