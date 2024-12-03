void StartUpCommands() {
  /********************************************************
    Initialize serial connection
  ********************************************************/
  Serial.begin(115200);

  /********************************************************
    Power up LD02
  ********************************************************/
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);

  /********************************************************
    Initialize I2C
  ********************************************************/
  if (!Wire.begin()) {
    Serial.println("I2C Error - check I2C Address");
  }

  /********************************************************
    Initialize GPIO expander
  ********************************************************/
  if (!mcp.begin_I2C()) {
    Serial.println("mcp error");
  }
  Serial.println("mcp ok");

  /********************************************************
    Initialize I2C on the second bus
  ********************************************************/
  if (!I2C_2.begin(SDA_2, SCL_2)) {
    Serial.println("I2C_2 Error - check I2C Address");
    while (1)
      ;
  }

  /********************************************************
    Initialize RTC
  ********************************************************/
  if (!rtc.begin(&I2C_2)) {
    Serial.println("Couldn't find RTC");
  }
  Serial.println("RTC started");

  /********************************************************
    Initialize Max17048
  ********************************************************/
  while (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!\n"));
  }
  Serial.print(F("Found MAX17048"));
  Serial.print(F(" with Chip ID: 0x"));
  Serial.println(maxlipo.getChipID(), HEX);

  /********************************************************
    Temp humidity sensor
  ********************************************************/
  Wire.begin();  // wait until serial monitor opens
  I2C_2.begin(SDA_2, SCL_2);

  if (!aht.begin(&I2C_2)) {
    Serial.println("Could not find AHT? Check wiring");
    delay(10);
  } else {
    Serial.println("AHT10 or AHT20 found");
  }

  /********************************************************
    Power up LDO3 and set pinmodes
  ********************************************************/
  mcp.pinMode(LDO3, OUTPUT);
  mcp.digitalWrite(LDO3, HIGH);

  mcp.pinMode(PG1, INPUT_PULLUP);

  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT_PULLUP);


  /********************************************************
    Stepper speed
  ********************************************************/
  stepper.setSpeed(36);

  /********************************************************
    Start side pixel
  ********************************************************/
  pixels.begin();

  /********************************************************
    Set up touch sensors
  ********************************************************/
  touch_pad_init();
  BaselineTouchSensors();
  CalibrateTouchSensors();
}