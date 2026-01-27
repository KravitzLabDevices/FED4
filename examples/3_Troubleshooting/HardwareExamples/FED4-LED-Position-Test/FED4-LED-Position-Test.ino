/*
 * FED4 LED Position Test
 * 
 * This sketch helps identify which physical LED corresponds to which position number.
 * It will:
 * 1. Light up each LED position (0-7) one at a time in sequence
 * 2. Show the position number in Serial Monitor
 * 3. Use different colors to make identification easier
 * 
 * Watch the LEDs and note which physical LED lights up for each position number.
 */

#include "FED4.h"

FED4 fed4;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("FED4 LED Position Test");
  Serial.println("======================");
  Serial.println("This test will light up each LED position one at a time.");
  Serial.println("Watch which physical LED lights up and note the position number.");
  Serial.println();
  
  // Use begin() to initialize all required components (LDOs, MCP, I2C, LED strip, etc.)
  // Pass nullptr to skip program-specific initialization
  Serial.println("Initializing FED4 (this may take a moment)...");
  if (!fed4.begin(nullptr)) {
    Serial.println("ERROR: Failed to initialize FED4!");
    while(1) delay(1000);
  }
  
  Serial.println("FED4 initialized successfully!");
  Serial.println("Starting position test in 2 seconds...");
  delay(2000);
}

void loop() {
  // Test 1: Light up each position individually with white
  Serial.println("\n--- Test 1: Individual positions (White) ---");
  for (int pos = 0; pos < 8; pos++) {
    Serial.print("Position ");
    Serial.print(pos);
    Serial.println(" - WHITE");
    
    // Clear all LEDs
    fed4.lightsOff();
    delay(100);
    
    // Light up only this position
    fed4.setStripPixel(pos, "white");
    delay(2000); // Hold for 2 seconds so you can see it
  }
  
  delay(1000);
  
  // Test 2: Light up each position with different colors
  Serial.println("\n--- Test 2: Individual positions (Different Colors) ---");
  const char* colors[] = {"red", "green", "blue", "yellow", "cyan", "purple", "orange", "white"};
  
  for (int pos = 0; pos < 8; pos++) {
    Serial.print("Position ");
    Serial.print(pos);
    Serial.print(" - ");
    Serial.println(colors[pos]);
    
    // Clear all LEDs
    fed4.lightsOff();
    delay(100);
    
    // Light up only this position with its color
    fed4.setStripPixel(pos, colors[pos]);
    delay(2000); // Hold for 2 seconds
  }
  
  delay(1000);
  
  // Test 3: Sequential chase to show order
  Serial.println("\n--- Test 3: Sequential chase (shows order) ---");
  Serial.println("Watch the LEDs light up in sequence to see the order");
  
  for (int pos = 0; pos < 8; pos++) {
    fed4.lightsOff();
    fed4.setStripPixel(pos, "red");
    delay(300);
  }
  
  delay(1000);
  
  // Test 4: Show all positions with numbers
  Serial.println("\n--- Test 4: All positions lit with position numbers ---");
  Serial.println("All LEDs are now lit. Check Serial Monitor for position numbers.");
  
  for (int pos = 0; pos < 8; pos++) {
    fed4.setStripPixel(pos, colors[pos]);
    Serial.print("Position ");
    Serial.print(pos);
    Serial.print(" = ");
    Serial.println(colors[pos]);
  }
  
  delay(5000);
  
  // Clear and wait before repeating
  fed4.lightsOff();
  Serial.println("\n--- Test complete. Restarting in 3 seconds... ---\n");
  delay(3000);
}
