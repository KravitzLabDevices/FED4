/********************************************************
  Update display
********************************************************/
void UpdateDisplay() {
  SerialOutput();
  display.begin();
  display.refresh();
  display.setRotation(2);  // Normal 1:1 pixel scale
  display.setTextColor(BLACK);
  display.setTextSize(3);  // Normal 1:1 pixel scale
  display.clearDisplay();
  display.setCursor(12, 20);
  display.print("FED4");
  display.refresh();

  display.setTextSize(1);  // Normal 1:1 pixel scale
  display.setCursor(12, 56);
  display.print("Pellets: ");
  display.print(PelletCount);
  display.setCursor(12, 72);
  display.print("L:");
  display.print(LeftCount);
  display.print("   C:");
  display.print(CenterCount);
  display.print("   R:");
  display.print(RightCount);

  //print temp and humidity on screen
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);  // populate temp and humidity objects with fresh data
  display.setCursor(12, 90);
  display.print("Temp: ");
  display.print(temp.temperature, 1);
  display.print("C");
  display.print(" Hum: ");
  display.print(humidity.relative_humidity, 1);
  display.println("%");

  float cellVoltage = maxlipo.cellVoltage();
  float cellPercent = maxlipo.cellPercent();
  display.setCursor(12, 122);
  display.print("Fuel: ");
  display.print(cellVoltage, 1);
  display.print("V, ");
  display.print(cellPercent, 1);
  display.println("%");

  //print date and time
  display.setCursor(12, 140);
  DateTime now = rtc.now();
  display.print(now.month());
  display.print("/");
  display.print(now.day());
  display.print("/");
  display.print(now.year());
  display.print(" ");
  display.print(now.hour());
  display.print(":");
  if (now.minute() < 10) {
    display.print('0');
  }  // Trick to add leading zero for formatting
  display.print(now.minute());

  display.setCursor(12, 156);
  display.print("Unclear:");
  display.print(WakeCount);
  // Update the display with the new text
  display.refresh();
}