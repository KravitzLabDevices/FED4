#include "FED4.h"
// NOTE: YOU NEED A BATTERY PLUGGED IN FOR THE MOTOR TO RUN.

bool FED4::initializeMotor()
{
    pinMode(MOTOR_PIN_1, OUTPUT);
    pinMode(MOTOR_PIN_2, OUTPUT);
    pinMode(MOTOR_PIN_3, OUTPUT);
    pinMode(MOTOR_PIN_4, OUTPUT);
    stepper.setSpeed(MOTOR_SPEED);
    return true;
}

void FED4::releaseMotor()
{
    digitalWrite(MOTOR_PIN_1, LOW);
    digitalWrite(MOTOR_PIN_2, LOW);
    digitalWrite(MOTOR_PIN_3, LOW);
    digitalWrite(MOTOR_PIN_4, LOW);
}

void FED4::minorJamClear()
{
    Serial.println("MinorJam");
    stepper.step(200);
    delay(1000);
}

void FED4::majorJamClear()
{ // make this function also monitor the pellet well
    Serial.println("MajorJam");
    stepper.step(1000);
    delay(1000);
}

void FED4::vibrateJamClear()
{ // make this function also monitor the pellet well
    Serial.println("VibrateJam");
    for (int i = 0; i < 35; i++)
    {
        stepper.step(10);
        delay(10);
        stepper.step(-20);
        delay(10);
    }
}

void FED4::jammed(){
  fillRect (0, 0, 144, 17, DISPLAY_BLACK);
  
  setFont(&Org_01);
  setTextSize(2);
  setTextColor(DISPLAY_WHITE);

  setCursor(0, 9);
  print("DISPENSE ERR"); 
  refresh();
  releaseMotor();
  logData("DispenseError");
  
  while(1) {
    // Infinite loop to hang the program
    LDO2_OFF();
    enableAmp(false); 
    // put FED4 to sleep
    esp_light_sleep_start();
    LDO3_ON();
    LDO2_ON();
    enableAmp(true);
    checkButton2();
    delay(100);
  }
}

void FED4::hapticBuzz(uint8_t duration)
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}

void FED4::hapticDoubleBuzz(uint8_t duration)
{
    for (int i = 0; i < 2; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration);
    }
}

void FED4::hapticTripleBuzz(uint8_t duration)
{
    for (int i = 0; i < 3; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration);
    }
}