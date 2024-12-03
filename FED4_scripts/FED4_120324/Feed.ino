/********************************************************
  Feeding loop
********************************************************/
void Feed() {
  if (FeedReady == true) {
    PG1Read = mcp.digitalRead(PG1);

    Serial.println("Feeding!");
    while (PG1Read == 1) {  //while pellet well is empty
      stepper.step(-2);     // small movement
      delay(10);
      PG1Read = mcp.digitalRead(PG1);
      pelletReady = true;
    }

    if (pelletReady == true) {
      PelletCount++;
      pelletReady = false;
    }
    FeedReady = false;

    ReleaseMotor();
    SerialOutput();
    strcpy(Event, "PelletDrop");  // Change Event to "PelletDrop"
    // LogData();

    //log retrieval time
    unsigned long PelletDrop = millis();
    while (PG1Read == 0) {  //while pellet well is full
      BluePix();
      PG1Read = mcp.digitalRead(PG1);
      RetrievalTime = millis() - PelletDrop;
      if (RetrievalTime > 10000) {
        break;
      }
    }
    RedPix();
    strcpy(Event, "PelletTaken");  // Change Event to "CenterPoke"
    // LogData();
    RetrievalTime = 0;
  }
  UpdateDisplay();

  //rebaseline touch sensors every 5 pellets
  if (PelletCount % 5 == 0 and PelletCount > 1) {  // Check if divisible by 5
    BaselineTouchSensors();
  }
}

/********************************************************
  Release power to stepper to cut power consumption
********************************************************/
void ReleaseMotor() {
  //digitalWrite stepper pins low to reduce current through motor
  digitalWrite(46, LOW);
  digitalWrite(37, LOW);
  digitalWrite(21, LOW);
  digitalWrite(38, LOW);
}
