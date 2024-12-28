#include "FED4.h"

void FED4::sleep()
{
  // enter sleep
  Serial.println("Entering light sleep...");
  Serial.flush();
  LDO2_OFF();
  enableAmp(false); 

  esp_light_sleep_start();

  // after wake
  LDO2_ON();

  //turn on audio amp (this takes a bit of time to warm up)
  esp_err_t err = i2s_start(I2S_NUM_0);
  enableAmp(true); 
  interpretTouch(); // do first to capture accurate touch values -

  Serial.println("Woke up!");
  purplePix();
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