#include "FED4.h"

void FED4::initializeMotor()
{
    pinMode(MOTOR_PIN_1, OUTPUT);
    pinMode(MOTOR_PIN_2, OUTPUT);
    pinMode(MOTOR_PIN_3, OUTPUT);
    pinMode(MOTOR_PIN_4, OUTPUT);
    stepper.setSpeed(MOTOR_SPEED);
}

void FED4::releaseMotor()
{
    digitalWrite(MOTOR_PIN_1, LOW);
    digitalWrite(MOTOR_PIN_2, LOW);
    digitalWrite(MOTOR_PIN_3, LOW);
    digitalWrite(MOTOR_PIN_4, LOW);
}

void FED4::vibrate()
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(100);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}