/*
 * FED4 Activity Monitor
 * Lex Kravitz, Nov 2025
 * 
 * Features:
 * - Polls sensors for motion detection (rate specified by samplingInterval)
 * - Logs activity and environmental data to SD card (rate specified by logInterval)
 * - Displays real-time activity information with counts on screen and Serial monitor
 * - LED indicators: Red (no activity), Blue (activity detected) on center port
 *
 * NOTES: If sleep is disabled, FED4 will last ~2.5 days on a fully charged battery
 *        If you sleep for 6s between readings, FED4 will last ~____ days.
 */

#include "FED4.h"

FED4 fed4;

//User-configurable variables
const int samplingInterval = 3;  //in seconds
const int logInterval = 60;  //in seconds

//Activity monitoring variables
unsigned long lastActivityTime = 0;
unsigned long lastLEDUpdate = 0;
bool ledState = false;
unsigned long lastLogTime = 0;
unsigned long timeSinceLastLog;

void setup() {
  // Initialize FED4 with program name
  fed4.begin("ActivityMonitor");

  Serial.println("FED4 Activity Monitor!");
  Serial.println("Time,ActivityCount,Percentage");

  // Turn off sleepyLEDs so the LEDs on the front stay on to show activity
  fed4.sleepyLEDs = false;

  // Initialize display with activity monitor content
  fed4.displayActivityMonitor();

  fed4.sleepSeconds = samplingInterval;  
}



void loop() {
  // Call FED4 system functions
  fed4.updateDisplay();
  fed4.syncHublink();
  updateActivityLED();
  printActivityStatus();

  // Check if it's time to log, if so log data and print status report
  timeSinceLastLog = millis() - lastLogTime;
  if (timeSinceLastLog >= logInterval * 1000) {
    lastLogTime = millis(); // Capture time BEFORE slow operations
    fed4.serialStatusReport();
    fed4.logData("Activity");
  }
  
  // Calculate time to next log, sleeping between samples if far from log time, otherwise use small delay and recheck on the next loop
  timeSinceLastLog = millis() - lastLogTime;
  unsigned long timeToNextLog = (logInterval * 1000) - timeSinceLastLog;
  // Only sleep if we're far from log time, otherwise use small delay and recheck on the next loop
  if (timeToNextLog > samplingInterval * 1000) {
    fed4.sleep();
  } else {
    delay(10);
  }
} 



void updateActivityLED() {
  // Update activity tracking using FED4 variables
  if (fed4.motionDetected) {
    lastActivityTime = millis();
    fed4.motionDetected = false;  // Reset the flag after counting
  }

  unsigned long currentTime = millis();

  // Update LED
  if (currentTime - lastLEDUpdate >= 500) {
    lastLEDUpdate = currentTime;
    ledState = !ledState;

    // Check if activity occurred in last 1 second
    bool recentActivity = (currentTime - lastActivityTime) < 1000;

    if (recentActivity) {
      // Blue LED for activity - keep it lit solid
      fed4.centerLight("blue", 50);
    } else {
      // Red LED for no activity - keep it lit solid
      fed4.centerLight("red", 50);
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
    if (fed4.pollCount > 0) {
      currentPercentage = (float)fed4.motionCount / fed4.pollCount * 100.0;
    }

    Serial.printf("%02lu:%02lu,%d,%.1f\n",
                  minutes, seconds,
                  fed4.motionCount,
                  currentPercentage);
  }
}