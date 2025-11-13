#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep() {
  // Enable timer-based wake-up every N seconds
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000); // Convert sleepSeconds to microseconds
  noPix(); 
  startSleep();
  wakeUp();
}

// Prepares device for sleep mode by disabling components and entering light sleep
void FED4::startSleep() {
  // Wait for all touch pads to be released before sleeping
  while (true) {
    float leftDev = abs((float)touchRead(TOUCH_PAD_LEFT) / touchPadLeftBaseline - 1.0);
    float centerDev = abs((float)touchRead(TOUCH_PAD_CENTER) / touchPadCenterBaseline - 1.0);
    float rightDev = abs((float)touchRead(TOUCH_PAD_RIGHT) / touchPadRightBaseline - 1.0);
    
    if (leftDev < TOUCH_THRESHOLD && centerDev < TOUCH_THRESHOLD && rightDev < TOUCH_THRESHOLD) {
      break;
    }
    delay(1);
  }

  // Calibrate touch sensors before sleep on every N wake-ups
  if (wakeCount % 20 == 0 )  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    
    // Add delay and I2C recovery after touch calibration to prevent I2C bus issues
    delay(1);  // Give I2C bus time to stabilize (reduced from 100ms)
    I2C_2.begin(SDA_2, SCL_2);  // Reinitialize secondary I2C bus
  }

  // Reset all touch flags before going to sleep
  resetTouchFlags();

  Serial.flush();
  
  // Check if sleepyLEDs flag is enabled
  if (sleepyLEDs) {
    lightsOff(); // clear the front LED strip
    noPix();  // Turn off the LED when going to sleep
    LDO3_OFF();  // Turn off LDO3 to power down NeoPixel
  }

  LDO2_OFF(); // turn off LDO2 every sleep
  enableAmp(false);

  if (sleepSeconds > 0) {  //only sleep if sleepSeconds is greater than 0
    esp_light_sleep_start();
  } else {
    wakeUp();
  }
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  wakeCount++;
  redPix(1); //very dim red pix to indicate when FED4 is awake

  // Reinitialize I2C buses FIRST before any sensor operations
  LDO2_ON();
  Wire.begin();  // Reinitialize primary I2C
  I2C_2.begin(SDA_2, SCL_2);  // Reinitialize secondary I2C 
  
  // Reconfigure GPIO expander pins after wake-up
  mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);
  mcp.pinMode(EXP_LDO3, OUTPUT);
  
  LDO3_ON();  // Turn on LDO3 
  enableAmp(true);

  // Only check button and sensor polling if not woken up by touch
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) {
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
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TOUCHPAD) {
    interpretTouch();
  }
}

// Handles touch inputs and only checks them if a button was not pressed
void FED4::handleTouch() {
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);

  // Check if wake-up was caused by timer
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    wakePad = 0;  // Reset wake pad for timer wake-up
    return;
  }

  // Check if any buttons are pressed
  if (digitalRead(BUTTON_1) == 1 || digitalRead(BUTTON_2) == 1 || digitalRead(BUTTON_3) == 1) {
    // This is a button wake-up, skip touch interpretation
    Serial.println("Button wake-up");
    wakePad = 0;  // Reset wake pad for button wake-up
    return;
  }

  // If we get here, this is a touch pad wake-up, so interpret the touch
  interpretTouch();
}

// Initializes LDO (Low-Dropout Regulator) power control pins
bool FED4::initializeLDOs()
{
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO3_ON();
    return true;
}

// Enables LDO2 power rail
void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
    delayMicroseconds(100); // Minimum 50us stabilization time
}

// Disables LDO2 power rail
void FED4::LDO2_OFF()
{
    digitalWrite(LDO2_ENABLE, LOW);
}

// Enables LDO3 power rail
void FED4::LDO3_ON()
{
    mcp.digitalWrite(EXP_LDO3, HIGH);
    delayMicroseconds(100); // Minimum 50us stabilization time
}

// Disables LDO3 power rail
void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
}