#include <FED4.h>

FED4 fed;

void setup()
{
  Serial.begin(115200);
  fed.begin();
  pinMode (2, OUTPUT);
  pinMode (3, OUTPUT);
  pinMode (4, OUTPUT);
}

void loop()
{
  fed.feed();
  digitalWrite (2, HIGH);
  digitalWrite (3, HIGH);
  digitalWrite (4, HIGH);
  delay (300);
  digitalWrite (2, LOW);
  digitalWrite (3, LOW);
  digitalWrite (4, LOW);
  fed.enterLightSleep();
}