void CalibrateTouchSensors() {
  Serial.println("Touch sensor calibration");
  touchAttachInterrupt(TOUCH_PIN_1, onWakeUp, threshold);
  touchAttachInterrupt(TOUCH_PIN_5, onWakeUp, threshold);
  touchAttachInterrupt(TOUCH_PIN_6, onWakeUp, threshold);
  // Enable touchpad wakeup
  esp_sleep_enable_touchpad_wakeup();
}

void BaselineTouchSensors() {
  Serial.println("Re-baselining...");
  baseline1 = touchRead(TOUCH_PIN_1);
  baseline5 = touchRead(TOUCH_PIN_5);
  baseline6 = touchRead(TOUCH_PIN_6);
  Serial.println("*****");
  Serial.printf("Pin 1: %d\n", baseline1);
  Serial.printf("Pin 5: %d\n", baseline5);
  Serial.printf("Pin 6: %d\n", baseline6);
  Serial.println("*****");
}

// Function to print touch data and which pin triggered wake-up
void interpretTouch() {
  //read touchpins, subtract baseline, and then take absolute value
  int reading1 = abs((int)(touchRead(TOUCH_PIN_1) - baseline1));
  int reading5 = abs((int)(touchRead(TOUCH_PIN_5) - baseline5));
  int reading6 = abs((int)(touchRead(TOUCH_PIN_6) - baseline6));

  Serial.println("Touch readings (baseline subtracted, abs):");
  Serial.printf("Pin 1: %d\n", reading1);
  Serial.printf("Pin 5: %d\n", reading5);
  Serial.printf("Pin 6: %d\n", reading6);

  // Determine the largest reading above the threshold
  int maxReading = 0;
  int triggeredPin = -1;

  if (reading1 > threshold && reading1 > maxReading) {
    maxReading = reading1;
    triggeredPin = 1;
  }

  if (reading5 > threshold && reading5 > maxReading) {
    maxReading = reading5;
    triggeredPin = 5;
  }

  if (reading6 > threshold && reading6 > maxReading) {
    maxReading = reading6;
    triggeredPin = 6;
  }

  // Act based on the pin with the largest reading
  if (triggeredPin == 1) {
    Serial.println("Wake-up triggered by Right Poke.");
    RightCount++;
    RedPix();

  } else if (triggeredPin == 5) {
    Serial.println("Wake-up triggered by Center Poke.");
    CenterCount++;
    BluePix();

  } else if (triggeredPin == 6) {
    Serial.println("Wake-up triggered by Left Poke.");
    GreenPix();
    LeftCount++;
    FeedReady = true;

  } else {
    Serial.println("Wake-up reason unclear, taking no action.");
    WakeCount++;
  }
}