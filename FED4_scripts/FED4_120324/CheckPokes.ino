/********************************************************
  Check the photogates
********************************************************/
void CheckForPellet() {
  PG1Read = mcp.digitalRead(PG1);
}

/********************************************************
  Check the touchpad
********************************************************/
void CheckTouch() {
  /////////////////////////////////////////////
  // Check the state of the touch pad
  /////////////////////////////////////////////

  // t = touchRead(TouchPad);
  // if (t < 40000) {
  //   Serial.println(", ...");
  // }
  // if (t >= 40000) {
  //   waketime = millis();
  //   Serial.println("Touched");
  //   Feed();
  // }
}