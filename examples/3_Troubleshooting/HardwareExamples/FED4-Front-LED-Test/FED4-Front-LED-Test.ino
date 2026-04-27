/*
  FED4 Front LED Strip Test

  Simple test for the 8 front NeoPixels around the nose-pokes.
  Cycles left/center/right groups and a full-strip wipe.
*/

#include <FED4.h>

FED4 fed4;
char task[] = "FrontLED_Test";

void setup() {
  Serial.begin(115200);

  // Optional: reduce startup time/noise for this test
  fed4.useMotionSensor = false;
  fed4.sleepyLEDs = false;

  if (!fed4.begin(task)) {
    Serial.println("FED4 begin() failed");
    while (true) { delay(1000); }
  }

  // Clear strip before starting
  fed4.lightsOff();
}

void loop() {
  fed4.leftLight("red");
  delay(500);

  fed4.centerLight("green");
  delay(500);

  fed4.rightLight("blue");
  delay(500);

  fed4.colorWipe("white", 10);
  delay(250);

  fed4.lightsOff();
  delay(500);
}
