/*
 * FED4 Motor Test (updated hardware)
 *
 * Uses the current motor pin mapping from src/FED4_Pins.h:
 *   MOTOR_PIN_1 = 38
 *   MOTOR_PIN_2 = 45
 *   MOTOR_PIN_3 = 46
 *   MOTOR_PIN_4 = 47
 *
 * NOTE: You need a battery connected for the motor driver to run reliably.
 * 28BYJ-48 + Arduino Stepper: use IN1,IN3,IN2,IN4 pin order (not 1,2,3,4).
 */

#include <Arduino.h>
#include <Stepper.h>
#include <FED4_Pins.h>

#define MOTOR_STEPS 2048   // 28BYJ-48 half-step, one output-shaft revolution
#define MOTOR_SPEED_RPM 8  // keep low on VBAT; increase once motion is reliable

Stepper stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_3, MOTOR_PIN_2, MOTOR_PIN_4);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=== FED4 Motor Test ===");
  Serial.println("Pins (Stepper order IN1,IN3,IN2,IN4): 38,46,45,47");

  stepper.setSpeed(MOTOR_SPEED_RPM);
}

void loop()
{
  Serial.println("Motor forward (1 rev)");
  stepper.step(MOTOR_STEPS);
  delay(1000);

  Serial.println("Motor reverse (1 rev)");
  stepper.step(-MOTOR_STEPS);
  delay(1000);
}
