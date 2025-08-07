/*
  Feeding experimentation device 4 (FED4) - Multi-Task Menu System
  
  This program demonstrates how to implement multiple behavioral tasks in a single FED4 sketch.
  The task type is selected through the FED4 menu system and stored in the meta.json file.
  The main loop checks which task is currently selected and runs the appropriate behavioral logic.
  
  Available Tasks:
  - Free_feeding: Mice can freely obtain pellets by touching any poke
  - FR1: Fixed Ratio 1 - One pellet per left poke touch
  - FR1_Approach: Fixed Ratio 1 with approach requirement - Mouse must approach pellet well after touching left poke
  
  This system allows researchers to easily switch between different experimental paradigms
  without needing to upload different sketches to the FED4 device.
  
  *** WARNING: If FED4 is their only source of food, ensure mice earn sufficient calories from 
  the task each day. For most mice this is ~150 20mg pellets (~3g of food) per 24 hours. *** 
*/

#include <FED4.h>           // Include the FED4 library
FED4 fed4;                  // Create FED4 object instance
char task[] = "FED4_Menu";  // Give this sketch a unique name for identification

void setup() {
  fed4.begin();  // Initialize FED4 hardware and systems (don't override menu setting)
}

void loop() {
  // Main FED4 run function - handles display updates, sleep management, and system maintenance
  fed4.run();
  
  // Get the currently selected task from the meta.json file
  // This allows the task to be changed via the menu without re-uploading code
  String currentTask = fed4.getMetaValue("fed", "program");
  
  // Debug output - print the current task every 10 seconds
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 10000) {  // Print every 10 seconds
    Serial.print("Current task detected: '");
    Serial.print(currentTask);
    Serial.println("'");
    lastDebugTime = millis();
  }

  // ============================================================================
  // FREE FEEDING TASK
  // ============================================================================
  // In this task, mice can obtain pellets freely - if they take a pellet it will be replaced.
  // Touches are logged but no reward is given.
  if (currentTask == "Free_feeding") {
    // Log touch events for all pokes and feed when any poke is touched
    if (fed4.leftTouch) {  // If left poke is touched
      fed4.click();        // Audio click stimulus for feedback
      fed4.leftLight("blue");  // Visual feedback - light left poke blue
      fed4.logData("Left");    // Log the touch event
      fed4.feed();         // Feed when left poke is touched
      Serial.println("Free_feeding: Left touch detected, feeding");
    }
    if (fed4.centerTouch) {  // If center poke is touched
      fed4.click();          // Audio click stimulus for feedback
      fed4.centerLight("blue");  // Visual feedback - light center poke blue
      fed4.logData("Center");    // Log the touch event
      fed4.feed();           // Feed when center poke is touched
      Serial.println("Free_feeding: Center touch detected, feeding");
    }
    if (fed4.rightTouch) {  // If right poke is touched
      fed4.click();         // Audio click stimulus for feedback
      fed4.rightLight("blue");  // Visual feedback - light right poke blue
      fed4.logData("Right");    // Log the touch event
      fed4.feed();          // Feed when right poke is touched
      Serial.println("Free_feeding: Right touch detected, feeding");
    }
  }

  // ============================================================================
  // FIXED RATIO 1 (FR1) TASK
  // ============================================================================
  // In this task, mice must touch the left poke to receive a pellet
  // This is a simple operant conditioning task - one response = one reward
  else if (currentTask == "FR1") {
    if (fed4.leftTouch) {  // If left poke is touched
      fed4.lowBeep();      // 500Hz 200ms beep - signals correct response
      fed4.leftLight("blue");  // Visual feedback - light left poke blue
      fed4.logData("Left");    // Log the correct response
      fed4.feed();         // Deliver pellet as reward
      Serial.println("FR1: Left touch detected, feeding");
    }
    if (fed4.centerTouch) {  // If center poke is touched
      fed4.click();          // Audio click stimulus for feedback
      fed4.centerLight("blue");  // Visual feedback - light center poke blue
      fed4.logData("Center");    // Log the touch event (no reward)
      Serial.println("FR1: Center touch detected, no reward");
    }
    if (fed4.rightTouch) {  // If right poke is touched
      fed4.click();         // Audio click stimulus for feedback
      fed4.rightLight("blue");  // Visual feedback - light right poke blue
      fed4.logData("Right");    // Log the touch event (no reward)
      Serial.println("FR1: Right touch detected, no reward");
    }
  }

  // ============================================================================
  // FIXED RATIO 1 WITH APPROACH REQUIREMENT (FR1_Approach) TASK
  // ============================================================================
  // In this task, mice must touch the left poke AND then approach the pellet well
  // This adds a spatial component to the task - mice must learn to go to the food location
  else if (currentTask == "FR1_Approach") {
    if (fed4.leftTouch) {  // If left poke is touched
      fed4.lowBeep();      // 500Hz 200ms beep - signals correct initial response
      fed4.leftLight("blue");  // Visual feedback - light left poke blue
      fed4.centerLight("red");  // Light center area red to guide mouse to pellet well
      fed4.logData("Left");    // Log the initial touch
      Serial.println("FR1_Approach: Left touch detected, waiting for approach");

      // Check proximity sensor for approach behavior
      // Mouse must approach pellet well (<20mm) within the time limit to get reward
      unsigned long startTime = millis();
      int approachTime = 2;  // How many seconds does the mouse have to approach?
      
      // Monitor proximity sensor during the approach window
      while (millis() < (startTime + (approachTime * 1000))) {
        int proximity = fed4.prox();  // Get distance from proximity sensor (mm)
        
        if (proximity > 0 && proximity < 20) {  // If mouse is within 20mm of pellet well
          fed4.bopBeep();        // Success sound - different from initial beep
          fed4.centerLight("white");  // Change center light to white (success signal)
          fed4.logData("Approach");   // Log successful approach
          fed4.feed();           // Deliver pellet as reward
          Serial.println("FR1_Approach: Approach detected, feeding");
          return;  // Exit early since trial is complete
        }
        delay(10);  // Small delay to prevent excessive sensor polling
      }
      
      // If we reach here, the mouse didn't approach within the time limit
      fed4.logData("No_approach");  // Log failed approach
      Serial.println("FR1_Approach: No approach detected within time limit");
    }

    // Log touches on other pokes (no reward for these)
    if (fed4.centerTouch) {  // If center poke is touched
      fed4.click();          // Audio click stimulus for feedback
      fed4.centerLight("blue");  // Visual feedback - light center poke blue
      fed4.logData("Center");    // Log the touch event
      Serial.println("FR1_Approach: Center touch detected, no reward");
    }

    if (fed4.rightTouch) {  // If right poke is touched
      fed4.click();         // Audio click stimulus for feedback
      fed4.rightLight("blue");  // Visual feedback - light right poke blue
      fed4.logData("Right");    // Log the touch event
      Serial.println("FR1_Approach: Right touch detected, no reward");
    }
  }
  
  // If no task matches, log an error
  else {
    static unsigned long lastErrorTime = 0;
    if (millis() - lastErrorTime > 5000) {  // Print error every 5 seconds
      Serial.print("ERROR: Unknown task '");
      Serial.print(currentTask);
      Serial.println("' - defaulting to FR1 behavior");
      lastErrorTime = millis();
    }
    
    // Default to FR1 behavior if task is unknown
    if (fed4.leftTouch) {
      fed4.lowBeep();
      fed4.leftLight("blue");
      fed4.logData("Left");
      fed4.feed();
    }
    if (fed4.centerTouch) {
      fed4.click();
      fed4.centerLight("blue");
      fed4.logData("Center");
    }
    if (fed4.rightTouch) {
      fed4.click();
      fed4.rightLight("blue");
      fed4.logData("Right");
    }
  }
}