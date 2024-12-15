#include <FED4.h>

FED4 fed;

void setup()
{
  Serial.begin(115200);
  fed.begin();
}

void loop()
{
  fed.feed();
  fed.enterLightSleep();
}
