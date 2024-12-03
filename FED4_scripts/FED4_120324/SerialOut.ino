/********************************************************
  Write to Serial port
********************************************************/
void SerialOutput() {

  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print("    ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print("Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C");

  Serial.println();
  Serial.print("Pellets: ");
  Serial.print(PelletCount);
  Serial.print(" Center: ");
  Serial.print(CenterCount);
  Serial.print(" Left: ");
  Serial.print(LeftCount);
  Serial.print("  Right: ");
  Serial.println(RightCount);
}
