#include "FED4.h"

void FED4::sleep()
{
  // enter sleep
  Serial.println("Entering light sleep...");
  Serial.flush();
  clearStrip();
  LDO2_OFF();
  enableAmp(false); 

  // put FED4 to sleep
  esp_light_sleep_start();

  // power on LDO2
  LDO2_ON();

  // re-start port expander
  mcp.begin_I2C();

  //turn on audio amp (this takes a bit of time to warm up so we do it right after waking up)
  esp_err_t err = i2s_start(I2S_NUM_0);
  enableAmp(true);

  Serial.println("Woke up!");

  // interpret touch pad only if button 1 is not pressed
  // hopefully in future we'll do better with interpreting touch pad
  // but for now, this is a simple way to not count touches when button 1 is held
  // this is a bit of a hack, but it works for now
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  Serial.print("Button 1 current state: ");
  Serial.println(digitalRead(BUTTON_1) == 1 ? "pressed" : "idle");
  if (digitalRead(BUTTON_1) == 0) {
    interpretTouch();
  }
  
  // check if button 1 is held for 1 second, if so, feed
  int holdTime = 0;
  while (digitalRead(BUTTON_1) == 1) {
    leftTouch = false;
    centerTouch = false;
    rightTouch = false;
    delay(100);
    holdTime += 100;
    if (holdTime >= 1000) {
        bopBeep();
        Serial.println("Button 1 held for 1 second, feeding!");
        feed();
        break;
    }
  }    
}

bool FED4::initializeLDOs()
{
    pinMode(LDO2_ENABLE, OUTPUT);
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO2_ON();
    LDO3_ON();
    return true;
}

void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
}

void FED4::LDO2_OFF()
{
    digitalWrite(LDO2_ENABLE, LOW);
}

void FED4::LDO3_ON()
{
    mcp.digitalWrite(EXP_LDO3, HIGH);
    delay(10); // delay to allow LDO to stabilize !! [ ] check docs for actual LDO delay
}

void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
}