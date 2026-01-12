/*
  FR1 Sequence Training Configuration File
  
  This file contains all the configurable parameters for the sequence training task.
  Modify these values to customize the training protocol for your specific experiment.
*/

#ifndef FR1_SEQUENCE_CONFIG_H
#define FR1_SEQUENCE_CONFIG_H

// ============================================================================
// SEQUENCE CONFIGURATION
// ============================================================================

// Target sequence for the mouse to learn
// Use 'L' for Left poke, 'R' for Right poke, 'C' for Center poke
// Separate items with commas
const char* TARGET_SEQUENCE = "L,R,C,R,L,C";

// Length of the target sequence (count the number of items above)
const int SEQUENCE_LENGTH = 6;

// ============================================================================
// TRAINING PARAMETERS
// ============================================================================

// Number of pellets the mouse must earn before increasing sequence complexity
const int PELLETS_PER_LEVEL = 50;

// ============================================================================
// AUDIO FEEDBACK SETTINGS
// ============================================================================

// Enable/disable audio feedback for correct sequences
const bool ENABLE_CORRECT_BEEP = true;

// Enable/disable audio feedback for incorrect sequences  
const bool ENABLE_ERROR_CLICK = true;

// Enable/disable haptic feedback for incorrect sequences
const bool ENABLE_ERROR_HAPTIC = true;

// Enable/disable level increase notification sound
const bool ENABLE_LEVEL_NOTIFICATION = true;

// ============================================================================
// VISUAL FEEDBACK SETTINGS
// ============================================================================

// Colors for each poke (can be: "red", "green", "blue", "yellow", "purple", "cyan", "white", "orange")
const char* LEFT_COLOR = "red";
const char* CENTER_COLOR = "green"; 
const char* RIGHT_COLOR = "blue";

// ============================================================================
// LOGGING SETTINGS
// ============================================================================

// Enable detailed logging of sequence progress
const bool ENABLE_DETAILED_LOGGING = true;

// Log sequence attempts (both correct and incorrect)
const bool LOG_SEQUENCE_ATTEMPTS = true;

// ============================================================================
// ADVANCED SETTINGS
// ============================================================================

// Maximum number of sequence attempts before forcing a reset (0 = no limit)
const int MAX_SEQUENCE_ATTEMPTS = 0;

// Enable debugging output to serial monitor
const bool ENABLE_DEBUG = false;

#endif // FR1_SEQUENCE_CONFIG_H
