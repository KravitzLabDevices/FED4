#include "FED4.h"

void FED4::sleep()
{
  // enter sleep
  Serial.println("Entering light sleep...");
  Serial.flush();
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

  // return which touch pad woke FED4 
  interpretTouch();

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