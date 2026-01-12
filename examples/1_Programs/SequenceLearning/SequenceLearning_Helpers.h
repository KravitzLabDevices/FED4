/*
  SequenceLearning_Helpers.h
  
  Helper functions for the Sequence Learning program.
  This file contains utility functions for sequence management, validation,
  and feedback handling.
*/

#ifndef SEQUENCE_LEARNING_HELPERS_H
#define SEQUENCE_LEARNING_HELPERS_H

#include <FED4.h>


// ============================================================================
// SEQUENCE MANAGER CLASS
// ============================================================================

class SequenceManager {
private:
  FED4& fed4;
  
  // Core sequence settings
  const char* targetSequence;
  int sequenceLength;
  int pelletsPerLevel;
  unsigned long timeoutMs;
  bool lightCuesEnabled;
  
  // State variables
  char* currentSequence;
  int sequenceIndex;
  int currentLevel;
  int pelletsAtCurrentLevel;
  unsigned long lastPokeTime;

  // Helper functions
  char pokeToChar(const char* poke) {
    if (strcmp(poke, "Left") == 0) return 'L';
    if (strcmp(poke, "Right") == 0) return 'R';
    if (strcmp(poke, "Center") == 0) return 'C';
    return '?';
  }

  bool checkSequence() {
    for (int i = 0; i < currentLevel; i++) {
      if (currentSequence[i] != targetSequence[i]) {
        return false;
      }
    }
    return true;
  }

  void resetSequence() {
    sequenceIndex = 0;
    for (int i = 0; i < sequenceLength; i++) {
      currentSequence[i] = '\0';
    }
    // Update display with reset sequence information
    String cleanSequence = getCleanTargetSequence();
    fed4.setSequenceDisplay(cleanSequence, sequenceIndex, currentLevel);
    // Cue the first poke in the sequence
    if (lightCuesEnabled) {
      cueNextPoke();
    }
  }

public:
  SequenceManager(FED4& f4, const char* targetSeq, int pelletsPerLvl, int timeoutSeconds, int startLevel = 1, bool lightCues = true) 
    : fed4(f4), targetSequence(targetSeq), 
      pelletsPerLevel(pelletsPerLvl), timeoutMs(timeoutSeconds * 1000), lightCuesEnabled(lightCues), sequenceIndex(0), currentLevel(startLevel), 
      pelletsAtCurrentLevel(0), lastPokeTime(0) {
    // Calculate sequence length automatically
    sequenceLength = strlen(targetSequence);
    currentSequence = new char[sequenceLength];
    resetSequence();
  }

  ~SequenceManager() {
    delete[] currentSequence;
  }

  void checkTimeout() {
    if (sequenceIndex > 0) {
      unsigned long currentTime = millis();
      if (currentTime - lastPokeTime > timeoutMs) {
        // Timeout occurred - reset sequence
        fed4.click();  // Use click instead of noise for timeout
        fed4.hapticBuzz();
        fed4.logData("Sequence_Timeout");
        // Don't reset counters on timeout - only reset when level increments
        resetSequence();
      }
    }
  }

  void addToSequence(const char* poke) {
    // Update timeout timer
    lastPokeTime = millis();
    
    // Set FR value to current level
    fed4.FR = currentLevel;
    
    // Update display with current sequence information
    String cleanSequence = getCleanTargetSequence();
    fed4.setSequenceDisplay(cleanSequence, sequenceIndex, currentLevel);
    
    if (sequenceIndex < sequenceLength) {
      char currentPoke = pokeToChar(poke);
      
      currentSequence[sequenceIndex] = currentPoke;
      sequenceIndex++;
      
      // Check if we've completed the current level
      if (sequenceIndex >= currentLevel) {
        if (checkSequence()) {
          // Log progress for the final poke only if sequence is correct
          String logMsg = poke;
          logMsg += " (";
          logMsg += sequenceIndex;
          logMsg += "/";
          logMsg += currentLevel;
          logMsg += ")";
          fed4.logData(logMsg);
          // Correct sequence! Give reward
          fed4.blockPokeCount++;  // Increment block poke count for final successful poke
          fed4.bopBeep();  // Special beep for completed sequence
          fed4.feed();
          fed4.blockPelletCount++;  // Increment block pellet count for completed sequence
          
          // Update level tracking
          pelletsAtCurrentLevel++;
          if (pelletsAtCurrentLevel >= pelletsPerLevel && currentLevel < sequenceLength) {
            currentLevel++;
            pelletsAtCurrentLevel = 0;
            fed4.highBeep();  // Signal level increase
            fed4.logData("Level_Increase");
            // Reset counters when level changes
            fed4.blockPelletCount = 0;
            fed4.blockPokeCount = 0;
            // Update display with new level
            String cleanSequence = getCleanTargetSequence();
            fed4.setSequenceDisplay(cleanSequence, sequenceIndex, currentLevel);
          }
          
          // Reset sequence for next trial
          resetSequence();
        } else {
          // Wrong sequence! Reset and give feedback
          fed4.click();  // First click
          delay(100);
          fed4.click();  // Second click
          delay(100);
          fed4.click();  // Third click
          fed4.hapticBuzz();
          fed4.logData(String(poke) + " (Error)");
          // Don't reset counters on error - only reset when level increments
          resetSequence();
        }
      } else {
        // Give immediate feedback for individual pokes
        // Check if this poke is correct for the current position
        if (currentPoke == targetSequence[sequenceIndex - 1]) {
          // Correct poke for this position
          fed4.blockPokeCount++;  // Increment block poke count for successful poke
          
          // Log progress only for correct pokes
          String logMsg = poke;
          logMsg += " (";
          logMsg += sequenceIndex;
          logMsg += "/";
          logMsg += currentLevel;
          logMsg += ")";
          fed4.logData(logMsg);
          
          fed4.lowBeep();  // Low beep for correct poke
          
          // Cue the next poke in the sequence
          if (lightCuesEnabled) {
            cueNextPoke();
          }

        } else {
          // Wrong poke - immediate feedback and reset
          fed4.click();  // First click
          delay(100);
          fed4.click();  // Second click
          delay(100);
          fed4.click();  // Third click
          fed4.hapticBuzz();
          fed4.logData(String(poke) + " (Error)");
          // Don't reset counters on error - only reset when level increments
          resetSequence();
        }
      }
    }
  }

  // Getter methods for external access if needed
  int getCurrentLevel() const { return currentLevel; }
  int getSequenceIndex() const { return sequenceIndex; }
  int getPelletsAtCurrentLevel() const { return pelletsAtCurrentLevel; }
  int getSequenceLength() const { return sequenceLength; }
  
  // Force level advancement (for button boost functionality)
  void sequenceAdvance() {
    if (currentLevel < sequenceLength) {
      currentLevel++;
      pelletsAtCurrentLevel = 0;
      // Update display with new level
      String cleanSequence = getCleanTargetSequence();
      fed4.setSequenceDisplay(cleanSequence, sequenceIndex, currentLevel);
    }
  }
  
  // Get clean target sequence for logging
  String getCleanTargetSequence() const {
    return String(targetSequence);
  }
  
  // Cue the next poke in the sequence (public method for external access)
  void cueNextPoke() {
    if (sequenceIndex < currentLevel) {
      char nextPoke = targetSequence[sequenceIndex];
      switch (nextPoke) {
        case 'L':
          fed4.leftLight("yellow", 10);  // Use for cueing
          break;
        case 'C':
          fed4.centerLight("cyan", 10);  // Use for cueing
          break;
        case 'R':
          fed4.rightLight("purple", 10);  // Use for cueing
          break;
        default:
          fed4.lightsOff();
          break;
      }
      // Small delay to ensure LED state is properly set
      delay(1);
    } else {
      fed4.lightsOff();  // Turn off lights if sequence is complete
    }
  }
};

#endif // SEQUENCE_LEARNING_HELPERS_H
