#include <FED4.h>

FED4 fed;

void setup() {
  Serial.begin(115200);
  fed.begin();
  // fed.playStartup();
}

void loop() {
  if (fed.leftTouch) {
    fed.feed();
  }

  fed.sleep();
}
