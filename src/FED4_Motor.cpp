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
  //turn lEDS off
  lightsOff();
  noPix();

  while(1) {
    // Infinite loop to hang the program
    //LDO2_OFF();  // For now leave this On until we get new boards

    enableAmp(false); 

    // put FED4 to sleep with timer wakeup for sensor polling
    esp_sleep_enable_timer_wakeup(10 * 1000000); // Wake up every 10 seconds (in microseconds)
    esp_light_sleep_start();
    LDO3_ON();
//    LDO2_ON();
    enableAmp(true);
    // Reinitialize I2C buses for sensor polling
    Wire.begin();
    I2C_2.begin(SDA_2, SCL_2);
    delay(1);

    // Poll sensors every wakeup (pollSensors checks internally if enough time has passed)
    if (program == "ActivityMonitor") {
      pollSensors(1);  // 1 minute interval for ActivityMonitor
    } else {
      pollSensors(10);  // 10 minute interval for other programs
    }
    checkButton2(); //check this button for resetting the device
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