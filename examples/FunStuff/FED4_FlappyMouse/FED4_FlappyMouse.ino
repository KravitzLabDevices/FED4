/*
  Flappy Mouse
  Modified to run on FED4 from:

  Created in 2016 by Maximilian Kern
  https://hackaday.io/project/12876-chronio
  Low power Arduino based (smart)watch code

  License: GNU GPLv3

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Adafruit_GFX.h>
#include <Adafruit_SharpMem.h>
#include "SPI.h"
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

// any pins can be used
#define SHARP_SCK 12
#define SHARP_MOSI 11
#define SHARP_SS 17

Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168);
static const uint8_t DISPLAY_BLACK = 0;
static const uint8_t DISPLAY_WHITE = 1;
int minorHalfSize;

#define btn1 14
#define haptic 8

//Initialize hiscore
byte hiscore;
bool scoreTrigger = true;

//Setup
void setup() {
  Serial.begin(115200);
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);

  display.begin();

  minorHalfSize = min(display.width(), display.height()) / 2;

  display.setRotation(2);  //Rotate screen
  display.setTextSize(2);
  display.clearDisplay();
  display.refresh();
  pinMode(btn1, INPUT);

  if (!mcp.begin_I2C()) {
    Serial.println("Error.");
    while (1)
      ;
  }
  delay(500);
  mcp.pinMode(haptic, OUTPUT);
  mcp.digitalWrite(haptic, LOW);
}

//Loop
void loop() {
  Flappy();
}

void Flappy() {
  mcp.digitalWrite(haptic, LOW);

  //  display.clearDisplay();
  static boolean firstRunGames = true;
  static float ySpeed = 0;
  static int py = 20;
  static boolean gameOver = false;
  static byte score = 0;
  static int wallPos[2] = { 100, 144 };
  static int wallGap[3] = { 60, 80, 20 };
  static unsigned long lastTime = millis();
  int wallsize = 40;

  if (firstRunGames) {
    ySpeed = score = 0;
    py = 20;
    wallPos[0] = 100;
    wallPos[1] = 186;
    lastTime = millis();
    gameOver = false;
  }

  float deltaTime = float(millis() - lastTime);

  //Mouse dropping speed
  ySpeed += deltaTime / 80;

  //Mouse Position
  py += ySpeed;
  py = constrain(py, 20, 130);

  //Clear mouse column on left side of screen
  display.fillRect(0, 0, 27, 168, DISPLAY_WHITE);

  //Draw walls and landscape
  for (int i = 0; i < 2; i++) {  // draw walls
    //clear landscape

    //Clear walls
    display.fillRect(wallPos[i], 18, 15, wallGap[i] - 18, DISPLAY_WHITE);
    display.fillRect(wallPos[i], wallGap[i] + wallsize, 15, 168, DISPLAY_WHITE);

    //Draw walls
    display.fillRect(wallPos[i] - 10, 18, 10, wallGap[i] - 18, DISPLAY_BLACK);
    display.fillRect(wallPos[i] - 10, wallGap[i] + wallsize, 10, 168, DISPLAY_BLACK);

    // draw mountains
    display.drawTriangle(23, 168, 32, 120, 45, 168, DISPLAY_BLACK);   //mountain 1
    display.fillTriangle(33, 168, 42, 130, 55, 168, DISPLAY_BLACK);   //mountain 2
    display.drawTriangle(53, 168, 65, 150, 75, 168, DISPLAY_BLACK);   //mountain 3
    display.fillTriangle(60, 168, 76, 125, 85, 168, DISPLAY_BLACK);   //mountain 4
    display.drawTriangle(80, 168, 92, 145, 105, 168, DISPLAY_BLACK);   //mountain 5
    display.drawTriangle(110, 168, 125, 130, 135, 168, DISPLAY_BLACK);   //mountain 7
    display.fillTriangle(120, 168, 142, 118, 159, 168, DISPLAY_BLACK);   //mountain 8

    //Check if mouse hit wall
    if (wallPos[i] > 5 && wallPos[i] < 20) {                                       // detect wall
      if (wallGap[i] > py + 5 || wallGap[i] < py - wallsize + 2) gameOver = true;  //detect gap
    }

    //When wall reaches left side reset it
    if (wallPos[i] <= 5) {  // reset wall
      wallPos[i] += 180;
      wallGap[i] = random(20, 100);
      scoreTrigger = true;
    }

    //When wall reaches left side reset it
    if ((wallPos[i] <= 20) and (scoreTrigger == true)) {
      score++;
      scoreTrigger = false;
    }

    //Draw Mouse
    display.fillRoundRect(5, py, 16, 12, 4, DISPLAY_BLACK);  //body
    if (py % 2 == 0) {
      display.fillRoundRect(14, py - 1, 11, 6, 2, DISPLAY_BLACK);  //head
      display.fillRoundRect(12, py - 3, 6, 3, 1, DISPLAY_BLACK);   //ear
      display.fillRoundRect(18, py - 1, 2, 1, 1, DISPLAY_WHITE);   //eye
      display.drawFastHLine(0, py + 1, 6, DISPLAY_BLACK);          //tail
      display.drawFastHLine(0, py, 6, DISPLAY_BLACK);              //tail
      display.fillRoundRect(18, py + 9, 4, 4, 1, DISPLAY_BLACK);   //front foot
      display.fillRoundRect(3, py + 9, 4, 4, 1, DISPLAY_BLACK);    //back foot
    } else {
      display.fillRoundRect(15, py - 2, 11, 7, 2, DISPLAY_BLACK);  //head
      display.fillRoundRect(13, py - 3, 6, 3, 1, DISPLAY_BLACK);   //ear
      display.fillRoundRect(19, py - 1, 2, 1, 1, DISPLAY_WHITE);   //eye
      display.drawFastHLine(0, py + 3, 6, DISPLAY_BLACK);          //tail
      display.drawFastHLine(0, py + 2, 6, DISPLAY_BLACK);          //tail
      display.fillRoundRect(16, py + 9, 4, 4, 1, DISPLAY_BLACK);   //front foot
      display.fillRoundRect(5, py + 9, 4, 4, 1, DISPLAY_BLACK);    //back foot
    }

    //Control wall speed  (lower the divisor number to speed up).  This will automatically speed up to a pretty impossible rate as score goes up to 11
    if (score < 4) wallPos[i] -= deltaTime / (20 - (score * 3));  // move walls

    Serial.println(wallPos[0]);
    Serial.println(wallPos[1]);

    if (score >= 4) wallPos[i] -= deltaTime / 9;  // move walls
  }

  //Display scores
  display.setTextColor(DISPLAY_BLACK, DISPLAY_WHITE);
  display.setCursor(3, 2);
  display.println(score);
  display.setCursor(68, 2);
  display.print(F("HI: "));
  display.println(hiscore);

  //Check for button press
  if (digitalRead(btn1) == HIGH) {
    ySpeed = -2;
    mcp.digitalWrite(haptic, HIGH);
    delay(5);
  }

  lastTime = millis();

  if (gameOver) {
    mcp.digitalWrite(haptic, HIGH);
    delay(100);
    mcp.digitalWrite(haptic, LOW);

    firstRunGames = true;
    display.clearDisplay();
    display.setCursor(15, 30);
    display.println(F("GAME OVER"));
    display.setCursor(20, 50);
    display.print(F("SCORE:"));
    display.println(score);
    if (score > hiscore) {
      display.setCursor(15, 110);
      display.print(F("HIGH SCORE"));
      hiscore = score;
    }
    display.setCursor(20, 70);
    display.print(F("HIGH: "));
    display.println(hiscore);
    display.refresh();
    delay(2500);

    display.clearDisplay();
  } else firstRunGames = false;

  display.refresh();
}
