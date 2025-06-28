/*
  ToF Sensor LED Color Example
  
  This sketch reads the ToF sensor for 1 second and changes the side LED color
  based on distance values:
  - Red: < 30mm
  - Blue: > 100mm  
  - Smooth transitions in between
  
  Hardware:
  - FED4 device with VL53L1X ToF sensor
  - Side LED strip for color output
*/

#include "FED4.h"

FED4 fed4;

// Distance thresholds for color mapping
const int MIN_DISTANCE = 30;   // Red threshold (mm)
const int MAX_DISTANCE = 100;  // Blue threshold (mm)
const int LOOP_DURATION = 1000; // 1 second in milliseconds

void setup() {
  // Initialize FED4
  if (!fed4.begin("ToF_LED_Example")) {
    Serial.println("FED4 initialization failed!");
    while (1);
  }
  
  Serial.println("ToF LED Color Example Started");
  Serial.println("Reading sensor for 1 second and updating LED color...");
}

void loop() {
  // Read ToF sensor for 1 second and update LED color
  readToFAndUpdateLED();
  
  // Wait a bit before next reading cycle
  delay(2000);
}

void readToFAndUpdateLED() {
  unsigned long startTime = millis();
  unsigned long endTime = startTime + LOOP_DURATION;
  
  Serial.println("Starting 1-second ToF reading cycle...");
  
  while (millis() < endTime) {
    // Read distance from ToF sensor
    int distance = fed4.readToFDistance();
    
    if (distance >= 0) {
      // Update LED color based on distance
      updateLEDColor(distance);
      
      // Print status
      Serial.printf("Distance: %d mm, ", distance);
      if (distance < MIN_DISTANCE) {
        Serial.println("Color: Red");
      } else if (distance > MAX_DISTANCE) {
        Serial.println("Color: Blue");
      } else {
        Serial.println("Color: Transition");
      }
    } else {
      Serial.println("ToF sensor error - keeping current LED color");
    }
    
    // Small delay between readings
    delay(50); // 20 readings per second
  }
  
  Serial.println("1-second reading cycle complete");
}

void updateLEDColor(int distance) {
  if (distance < MIN_DISTANCE) {
    // Red for close objects
    fed4.colorWipe("red", 0); // 0 delay for immediate update
  } else if (distance > MAX_DISTANCE) {
    // Blue for far objects
    fed4.colorWipe("blue", 0); // 0 delay for immediate update
  } else {
    // Smooth transition between red and blue
    float ratio = (float)(distance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE);
    
    // Interpolate between red (255,0,0) and blue (0,0,255)
    uint8_t red = (uint8_t)(255 * (1.0 - ratio));
    uint8_t blue = (uint8_t)(255 * ratio);
    
    // Create custom color using RGB values
    uint32_t customColor = (red << 16) | (0 << 8) | blue; // RGB format
    
    // Update all LEDs on the strip with the new color
    fed4.colorWipe(customColor, 0); // 0 delay for immediate update
  }
} 