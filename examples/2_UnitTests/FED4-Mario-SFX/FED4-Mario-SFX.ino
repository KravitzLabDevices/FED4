#include <FED4.h>

FED4 fed4;

void setup() {
  Serial.begin(115200);
  fed4.begin("FED4-Mario-SFX");

  delay(500);
  Serial.println("Playing Mario SFX demo (coin, jump, pipe, fireball, mushroom)...");
}

void loop() {
  fed4.marioCoin();
  delay(600);

  fed4.marioJump();
  delay(600);

  fed4.marioPipe();
  delay(700);

  fed4.marioFireball();
  delay(700);

  fed4.marioMushroom();
  delay(1200);
}

