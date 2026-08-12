#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep(int seconds) {
  sleepSeconds = seconds;
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000); // Convert sleepSeconds to microseconds
  noPix(); 
  startSleep();
  wakeUp();
}

// For backward compatibility, keep the parameterless version
void FED4::sleep() {
  sleep(sleepSeconds);
}

// Prepares device for sleep mode by disabling components and entering light sleep
void FED4::startSleep() {
  // Wait for all touch pads to be released before sleeping
  while (!fed4TouchPadsReleased(TOUCH_THRESHOLD)) {
    delay(1);
  }

  // Calibrate touch sensors before sleep on every N wake-ups, unless program is ActivityMonitor
  if (program != "ActivityMonitor" && wakeCount % 200 == 0)  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    delay(1);  // Give I2C bus time to stabilize
  }

  // Reset all touch flags before going to sleep
  resetTouchFlags();
  wakePad = 0;

  Serial.flush();
  
  // Check if sleepyLEDs flag is enabled
  if (sleepyLEDs) {
    lightsOff(); // clear the front LED strip
    noPix();  // Turn off status LED when going to sleep
    PSV3_OFF();  // Turn off PSV3 to power down front RGB strip
  }
  // PSV2 remains ON during sleep (RTC, photogates, haptic on PSV2 rail)

  enableAmp(false);

  fed4TouchEnableTouchpadWakeup();

  if (sleepSeconds > 0) {  //only sleep if sleepSeconds is greater than 0
    esp_light_sleep_start();
  } else {
    wakeUp();
  }
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  wakeCount++;

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  // Clear wakePad for non-touch wake-ups
  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD) {
    wakePad = 0;
  }
  
  redPix(1); //very dim red pix to indicate when FED4 is awake

  // Reinitialize I2C bus FIRST before any sensor operations
  PSV2_ON();
  Wire.begin();  // Reinitialize primary I2C
  delay(1);  // Brief delay after I2C init
  
  // Reconfigure GPIO expander pins after wake-up
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  PSV3_ON();  // Turn on PSV3 (front RGB strip)
  enableAmp(true);

  // If woken by INT_OR (GPIO wake, line LOW), scan interrupt sources so the
  // sketch can call getLastInterruptMask() after sleep() returns.
  // scanAndClearInterrupts() also releases the line so the next sleep is clean.
  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO && interruptPending()) {
    lastInterruptMask = scanAndClearInterrupts();
    Serial.printf("INT_OR wake: sources = 0x%02X\n", lastInterruptMask);
  } else {
    lastInterruptMask = INT_SRC_NONE;
  }

  // Only check button and sensor polling if not woken up by touch
  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD) {
    checkButton1();
    checkButton2(); 
    checkButton3();

    if (program == "ActivityMonitor") {
      pollSensors(1);  //default for activity monitoring is 1 minutes between sensor polling, change this here
    } else {
      pollSensors(10);  //default for all other programs is 10 minutes between sensor polling, change this here
    }
    
  }

  // Only check touch sensors if woken up by touch
  if (wakeCause == ESP_SLEEP_WAKEUP_TOUCHPAD) {
    interpretTouch();
  }
}

// Initializes PSV2 and PSV3 power switch rails via MCP expander
// PSV2 (3.3V2): RTC, amplifier, haptic, photogates (TCA4307 downstream)
// PSV3 (3.3V3): front RGB LED strip
bool FED4::initializePower()
{
    mcp.pinMode(EXP_PSV2_EN, OUTPUT);
    mcp.pinMode(EXP_PSV3_EN, OUTPUT);
    PSV2_ON();
    PSV3_ON();
    return true;
}

// Enables PSV2 power rail (RTC/amp/haptic/photogate)
void FED4::PSV2_ON()
{
    mcp.digitalWrite(EXP_PSV2_EN, LOW); // ~ON is active LOW
    delayMicroseconds(100); // Stabilization time
}

// Disables PSV2 power rail
void FED4::PSV2_OFF()
{
    mcp.digitalWrite(EXP_PSV2_EN, HIGH);
}

// Enables PSV3 power rail (front RGB strip)
void FED4::PSV3_ON()
{
    mcp.digitalWrite(EXP_PSV3_EN, LOW); // ~ON is active LOW
    delayMicroseconds(100); // Stabilization time
}

// Disables PSV3 power rail (front RGB strip)
void FED4::PSV3_OFF()
{
    mcp.digitalWrite(EXP_PSV3_EN, HIGH);
}