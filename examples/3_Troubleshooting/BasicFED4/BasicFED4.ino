#include <FED4.h>

FED4 fed4;

void setup() {
  Serial.begin(115200);
  fed4.begin();
  // fed4.playStartup();
}

void loop() {
  if (fed4.leftTouch) {
    fed4.feed();
  }

  fed4.sleep();
}
