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
 */

#include <Arduino.h>
#include <Stepper.h>

#define MOTOR_STEPS 512
#define MOTOR_SPEED_RPM 24

#define MOTOR_PIN_1 38
#define MOTOR_PIN_2 45
#define MOTOR_PIN_3 46
#define MOTOR_PIN_4 47

Stepper stepper(MOTOR_STEPS, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("=== FED4 Motor Test ===");
  Serial.println("Pins: 38,45,46,47");

  stepper.setSpeed(MOTOR_SPEED_RPM);
}

void loop() {
  Serial.println("Motor forward (1 rev)");
  stepper.step(MOTOR_STEPS);
  delay(1000);

  Serial.println("Motor reverse (1 rev)");
  stepper.step(-MOTOR_STEPS);
  delay(1000);
}

