/*
 * FED4 Photogate Test
 *
 * Photogates are direct GPIO inputs
 * Active LOW when beam is blocked.
 */

#include <Arduino.h>

#define PHOTOGATE_1 14 // Center
#define PHOTOGATE_2 17 // Left
#define PHOTOGATE_3 18 // Right
#define PHOTOGATE_4 37 // Pellet detector

int last1 = HIGH, last2 = HIGH, last3 = HIGH, last4 = HIGH;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(PHOTOGATE_1, INPUT_PULLUP);
  pinMode(PHOTOGATE_2, INPUT_PULLUP);
  pinMode(PHOTOGATE_3, INPUT_PULLUP);
  pinMode(PHOTOGATE_4, INPUT_PULLUP);

  Serial.println("=== FED4 Photogates Test ===");
}

void loop() {
  int pg1 = digitalRead(PHOTOGATE_1);
  int pg2 = digitalRead(PHOTOGATE_2);
  int pg3 = digitalRead(PHOTOGATE_3);
  int pg4 = digitalRead(PHOTOGATE_4);

  if (pg1 != last1 && pg1 == LOW) Serial.println("Center nose poke");
  if (pg2 != last2 && pg2 == LOW) Serial.println("Left nose poke");
  if (pg3 != last3 && pg3 == LOW) Serial.println("Right nose poke");
  if (pg4 != last4 && pg4 == LOW) Serial.println("Pellet detector blocked");

  last1 = pg1;
  last2 = pg2;
  last3 = pg3;
  last4 = pg4;

  delay(20);
}