#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep() {
  startSleep();
  wakeUp();
  handleTouch();
  checkButton1();
  checkButton2();
  checkButton3();
}

// Prepares device for sleep mode by disabling components and entering light sleep
void FED4::startSleep() {
  Serial.flush();
  clearStrip();
  LDO2_OFF();
  LDO3_OFF();  // LEDs are on this - could leave it on to keep them illuminated during sleep
  enableAmp(false);
  esp_light_sleep_start();
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  LDO2_ON();
  LDO3_ON();
  mcp.begin_I2C();
  esp_err_t err = i2s_start(I2S_NUM_0);
  enableAmp(true);
}

// Handles touch inputs and only checks them if a button was not pressed
void FED4::handleTouch() {
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);

  // if no button was pressed, check touch inputs to see which one woke FED4 
  if (digitalRead(BUTTON_1) == 0 && digitalRead(BUTTON_2) == 0 && digitalRead(BUTTON_3) == 0) {
    interpretTouch();
  }
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

// Checks if reset button is held and performs device reset after 2 seconds
void FED4::checkButton2() {
  int holdTime = 0;
  while (digitalRead(BUTTON_2) == 1) {
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
    delay(100);
    holdTime += 100;
    if (holdTime >= 2000) {
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
    pinMode(LDO2_ENABLE, OUTPUT);
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO2_ON();
    LDO3_ON();
    return true;
}

// Enables LDO2 power rail
void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
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
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
}

// Disables LDO3 power rail
void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
}