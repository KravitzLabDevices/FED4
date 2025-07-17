#include <FED4.h> //Include the FED3 library
FED4 fed;

#include <Hublink.h>
Hublink hublink(SD_CS); // Use default Hublink instance

void setup()
{
  Serial.begin(9600);
  delay(1000);
  Serial.println("Hello, Hublink.");

  fed.begin(); // inits SD card
  hublink.begin(); // Uses default callbacks
}

void loop()
{
  hublink.sync(); // only blocks when ready
  // fed.Feed();
  // fed.sleep();
  delay(1000);
  Serial.println("I'm alive");
}