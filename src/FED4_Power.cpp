#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep() {
  // Enable timer-based wake-up every 5 seconds
  esp_sleep_enable_timer_wakeup(6 * 1000000); // Convert 6 seconds to microseconds
  noPix(); 
  startSleep();
  wakeUp();
  redPix(50); //red pix to indicate FED4 is awake

  // Check sensors on timer wake-up and buttons
  handleTouch();
  checkButton1();
  checkButton2();
  checkButton3();
  pollSensors();
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
    delay(10);
  }

  // Calibrate touch sensors before sleep on every N wake-ups
  if (wakeCount % 10 == 0 )  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
  }

  Serial.flush();
  clearStrip();
  noPix();  // Turn off the LED when going to sleep
  LDO3_OFF();  // Turn off LDO3 to power down NeoPixel
  LDO2_OFF();
  enableAmp(false);
  esp_light_sleep_start();
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  wakeCount++;
  LDO2_ON();
  Wire.begin();  // Reinitialize I2C
  mcp.begin_I2C();  // Reinitialize MCP after I2C
  LDO3_ON();  // Turn on LDO3 to power up NeoPixel
  enableAmp(true);
}

// Handles touch inputs and only checks them if a button was not pressed
void FED4::handleTouch() {
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);

  // Check if wake-up was caused by timer
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    // This is a timer wake-up, skip touch interpretation
    Serial.print("Timer wake-up");
    wakePad = 0;  // Reset wake pad for timer wake-up
    touch_pad_clear_status();
    return;
  }

  // Check if any buttons are pressed
  if (digitalRead(BUTTON_1) == 1 || digitalRead(BUTTON_2) == 1 || digitalRead(BUTTON_3) == 1) {
    // This is a button wake-up, skip touch interpretation
    Serial.println("Button wake-up");
    wakePad = 0;  // Reset wake pad for button wake-up
    touch_pad_clear_status();
    return;
  }

  // If we get here, this is a touch pad wake-up, so interpret the touch
  interpretTouch();
}

// Checks if feed button is held and dispenses test pellet after 1 second
void FED4::checkButton1() {
  int holdTime = 0;
  while (digitalRead(BUTTON_1) == 1) {
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
    delay(100);
    holdTime += 100;
    if (holdTime >= 1000) {
        bopBeep();
        Serial.println("********** TEST PELLET DISPENSE **********");
        feed();
        break;
    }
  }
}

// Checks if reset button is held and performs device reset after 1 second
void FED4::checkButton2() {
  int holdTime = 0;
  while (digitalRead(BUTTON_2) == 1) {
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
    delay(100);
    holdTime += 100;
    if (holdTime >= 1000) {
        colorWipe("red", 100); // red
        resetJingle();
        Serial.println("********** BUTTON 2 FORCED RESET! **********");
        esp_restart();
        break;
    }
  }
}

// Checks if Button 3 is held and dispenses test pellet after 1 second
void FED4::checkButton3() {
  int holdTime = 0;
  while (digitalRead(BUTTON_3) == 1) {
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
    delay(100);
    holdTime += 100;
    if (holdTime >= 1000) {
        menuJingle();
        Serial.println("********** BUTTON 3 MENU START **********");
        menu();
        break;
    }
  }
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
    delay(5); // Minimum 50us stabilization time
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