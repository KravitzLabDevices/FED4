#include <Arduino.h>
#include <Stepper.h>

#define STEPS 512
Stepper stepper(STEPS, 46, 37, 21, 38);

void setup()
{

	stepper.setSpeed(12);
	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
}

void loop()
{

	// NOTE: YOU NEED A BATTERY PLUGGED IN FOR THE MOTOR TO RUN.

	stepper.step(STEPS); // Forward
	delay(1000);
	stepper.step(-STEPS); // Reverse
	delay(1000);
}

