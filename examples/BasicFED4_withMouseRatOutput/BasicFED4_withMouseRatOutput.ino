#include <FED4.h>

FED4 fed;

void setup() {
  Serial.begin(115200);
  fed.begin();
  pinMode(3, OUTPUT);
}

void loop() {
  fed.feed();

  //Pulse for MR1 needs to be >200ms long
  digitalWrite(3, HIGH);
  delay(300);
  digitalWrite(3, LOW);

  fed.enterLightSleep();
}