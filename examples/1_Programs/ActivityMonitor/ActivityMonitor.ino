/*
 * FED4 Activity Monitor
 * 
 * This program continuously monitors activity by calling pollSensors every second
 * with minToUpdateSensors set to 1 minute. This allows for frequent sensor updates
 * and comprehensive activity logging.
 * 
 * Features:
 * - Polls sensors every second for motion detection
 * - Updates environmental sensors every 1 minute
 * - Logs activity data to SD card
 * - Displays real-time activity information with counts on screen
 * - Center port LED indicators: Red (no activity), Blue (activity detected)
 * - Serial output for monitoring
 * 
 * Usage:
 * - Upload this sketch to your FED4 device
 * - The device will start monitoring activity immediately
 * - Data is logged to SD card with timestamps
 * - Press any button to view current status
 */

#include "FED4.h"

FED4 fed;

// Activity monitoring variables
unsigned long lastActivityTime = 0;
unsigned long lastLEDUpdate = 0;
bool ledState = false;

void setup() {
  // Initialize FED4 with program name
  if (!fed.begin("ActivityMonitor")) {
    Serial.println("Failed to initialize FED4");
    while(1) delay(1000);
  }
  
  Serial.println("FED4 Activity Monitor Started");
  Serial.println("Monitoring activity every second...");
  Serial.println("LED: Red = No Activity, Blue = Activity Detected");
  Serial.println("Time,Activity,Count,Percentage");
  
  // Set up initial display
  fed.sleepyLEDs = false;
  
  // Initialize display with activity monitor content
  fed.updateDisplay();
  
  // Log startup event
  fed.logData("Startup");
}

void loop() {
  // Poll sensors every second with 1-minute update interval
  fed.pollSensors(1);  // Update sensors every 1 minute
  
  // Update activity tracking using FED4 variables
  if (fed.motionDetected) {
    lastActivityTime = millis();
    fed.motionDetected = false; // Reset the flag after counting
  }
  
  // Update LED based on activity
  updateActivityLED();
  
  // Call FED4 system functions (including updateDisplay which now shows activity monitor)
  fed.updateDisplay();
  fed.syncHublink();
  
  // Serial output every second
  printActivityStatus();
  
  // Measure and print execution time for each loop iteration
  static unsigned long lastLoopEnd = 0;
  unsigned long loopStart = millis();
  static unsigned long loopCount = 0;

  delay(100); // Small delay to prevent excessive CPU usage

  unsigned long loopEnd = millis();
  unsigned long execTime = loopEnd - loopStart;
  Serial.print("Loop ");
  Serial.print(++loopCount);
  Serial.print(" execution time: ");
  Serial.print(execTime);
  Serial.println(" ms");
}

void updateActivityLED() {
  unsigned long currentTime = millis();
  
  // Update LED every 500ms
  if (currentTime - lastLEDUpdate >= 500) {
    lastLEDUpdate = currentTime;
    ledState = !ledState;
    
    // Check if activity occurred in last 1 second
    bool recentActivity = (currentTime - lastActivityTime) < 1000;
    
    if (recentActivity) {
      // Blue LED for activity - keep it lit solid
      fed.centerLight("blue", 50);
    } else {
      // Red LED for no activity - keep it lit solid
      fed.centerLight("red", 50);
    }
  }
}

void printActivityStatus() {
  static unsigned long lastSerialOutput = 0;
  unsigned long currentTime = millis();
  
  // Print status every second
  if (currentTime - lastSerialOutput >= 1000) {
    lastSerialOutput = currentTime;
    
    // Format time
    unsigned long seconds = currentTime / 1000;
    unsigned long minutes = seconds / 60;
    seconds = seconds % 60;
    
    // Check if there was recent activity (within last 1 second)
    bool recentActivity = (currentTime - lastActivityTime) < 1000;
    
    // Calculate real-time percentage using FED4 variables
    float currentPercentage = 0.0;
    if (fed.pollCount > 0) {
      currentPercentage = (float)fed.motionCount / fed.pollCount * 100.0;
    }
    
    Serial.printf("%02lu:%02lu,%s,%d,%.1f\n", 
                  minutes, seconds,
                  recentActivity ? "YES" : "NO",
                  fed.motionCount,
                  currentPercentage);
  }
}