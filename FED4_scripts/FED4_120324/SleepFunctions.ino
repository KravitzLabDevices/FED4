// Function to enter light sleep
void enterLightSleep() {
  // Configure wake-up source

  // Set up the RTC timer wakeup
  // esp_sleep_enable_timer_wakeup(1000);

  //Power everything down
  Serial.println("Powering down...");

  NoPix();
  digitalWrite(47, LOW);
  digitalWrite(36, LOW);

  // Go to sleep
  Serial.println("Entering light sleep...");
  delay(50);
  esp_light_sleep_start();

  // Code execution resumes here after wake-up
  Serial.println("Woke up!");

  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);

  pinMode(36, OUTPUT);
  digitalWrite(36, HIGH);
  interpretTouch();
  PurplePix();
  CalibrateTouchSensors();
}