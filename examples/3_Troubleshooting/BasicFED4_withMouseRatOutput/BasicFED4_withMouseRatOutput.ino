#include <FED4.h>

FED4 fed4;

void setup() {
  Serial.begin(115200);
  fed4.begin();
  pinMode(3, OUTPUT);
}

void loop() {
  fed4.feed();

  //Pulse for MR1 needs to be >200ms long
  digitalWrite(3, HIGH);
  delay(300);
  digitalWrite(3, LOW);

  fed4.enterLightSleep();
}