// Advanced STHS34PF80 infrared sensor test with button controls
// Button 1 (GPIO 40): Cycle through Low-Pass Filter settings
// Button 2 (GPIO 39): Adjust Motion Sensitivity (detection threshold)
// Button 3 (GPIO 14): Adjust Motion Hysteresis (prevents flickering)

#include "Adafruit_STHS34PF80.h"
#include <Wire.h>

// Define I2C_2 bus with FED4 pins
#define SDA_2 20
#define SCL_2 19

// Button pins (FED4 buttons)
#define BUTTON1_PIN 40  // Bottom button
#define BUTTON2_PIN 39  // Middle button
#define BUTTON3_PIN 14  // Top button

TwoWire I2C_2 = TwoWire(0);  // Use I2C peripheral 0
Adafruit_STHS34PF80 sths;

// Current settings indices
int lpfIndex = 2;  // Start at ODR/50 (user's preference)
const int ODR_30HZ = 7;  // Fixed at 30 Hz
int sensitivityIndex = 4;  // Start at middle sensitivity (0)
int hysteresisIndex = 3;  // Start at default hysteresis (50)

// Sensitivity options (range -128 to 127 for int8_t, lower = more sensitive)
// This sets the threshold for the built-in motion detection algorithm
// To change: modify sensitivityIndex above (0-8) or call cycleSensitivity()
int8_t sensitivityLevels[] = {
  -128, -96, -64, -32, 0, 32, 64, 96, 127
};
const char* sensitivityNames[] = {
  "Ultra (-128)", "V.High (-96)", "High (-64)", "Med-High (-32)", 
  "Medium (0)", "Med-Low (32)", "Low (64)", "V.Low (96)", "Min (127)"
};

// Hysteresis options (0-255, prevents flickering at threshold)
// Higher values = more hysteresis = more stable but less responsive
uint8_t hysteresisLevels[] = {0, 10, 25, 50, 75, 100, 150, 200, 255};
const char* hysteresisNames[] = {
  "None (0)", "V.Low (10)", "Low (25)", "Med-Low (50)", 
  "Medium (75)", "Med-High (100)", "High (150)", "V.High (200)", "Max (255)"
};

// LPF options array
sths34pf80_lpf_config_t lpfOptions[] = {
  STHS34PF80_LPF_ODR_DIV_9,
  STHS34PF80_LPF_ODR_DIV_20,
  STHS34PF80_LPF_ODR_DIV_50,
  STHS34PF80_LPF_ODR_DIV_100,
  STHS34PF80_LPF_ODR_DIV_200,
  STHS34PF80_LPF_ODR_DIV_400,
  STHS34PF80_LPF_ODR_DIV_800
};

const char* lpfNames[] = {
  "ODR/9", "ODR/20", "ODR/50", "ODR/100", "ODR/200", "ODR/400", "ODR/800"
};

// ODR options array
sths34pf80_odr_t odrOptions[] = {
  STHS34PF80_ODR_0_25_HZ,
  STHS34PF80_ODR_0_5_HZ,
  STHS34PF80_ODR_1_HZ,
  STHS34PF80_ODR_2_HZ,
  STHS34PF80_ODR_4_HZ,
  STHS34PF80_ODR_8_HZ,
  STHS34PF80_ODR_15_HZ,
  STHS34PF80_ODR_30_HZ
};

const char* odrNames[] = {
  "0.25 Hz", "0.5 Hz", "1 Hz", "2 Hz", "4 Hz", "8 Hz", "15 Hz", "30 Hz"
};

void setup() {
  // Power up sensor
  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);
  delay(100);
  
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  STHS34PF80 Advanced Configuration     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize buttons (FED4 uses pulldown, active HIGH)
  pinMode(BUTTON1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON2_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON3_PIN, INPUT_PULLDOWN);
  
  // Initialize I2C_2 with the correct pins
  I2C_2.begin(SDA_2, SCL_2);
  I2C_2.setClock(400000);  // Set to 400kHz (fast mode)
  
  // Initialize sensor
  if (!sths.begin(0x5A, &I2C_2)) {
    Serial.println("❌ STHS34PF80 NOT FOUND!");
    while(1) delay(100);
  }
  Serial.println("✅ STHS34PF80 Found on I2C_2!\n");
  
  // Simple initialization - just set ODR to start continuous mode
  Serial.println("Starting continuous measurement mode...");
  Serial.print("Setting ODR to 30 Hz... ");
  if (sths.setOutputDataRate(STHS34PF80_ODR_30_HZ)) {
    Serial.println("✅ Success");
  } else {
    Serial.println("❌ FAILED");
  }
  
  Serial.print("Setting LPF to ODR/50... ");
  if (sths.setMotionLowPassFilter(STHS34PF80_LPF_ODR_DIV_50)) {
    Serial.println("✅ Success");
  } else {
    Serial.println("❌ FAILED");
  }
  
  // Wait for first measurement
  Serial.println("\nWaiting for sensor to start (2 seconds)...");
  delay(2000);
  
  Serial.println("Ready! Data should now be streaming...\n");
  
  printSettings();
  printHelp();
  
  // Debug button state
  Serial.println("\n--- Button Debug Info ---");
  Serial.print("Button 1 (GPIO 40) state: ");
  Serial.println(digitalRead(BUTTON1_PIN) ? "HIGH" : "LOW");
  Serial.print("Button 2 (GPIO 39) state: ");
  Serial.println(digitalRead(BUTTON2_PIN) ? "HIGH" : "LOW");
  Serial.print("Button 3 (GPIO 14) state: ");
  Serial.println(digitalRead(BUTTON3_PIN) ? "HIGH" : "LOW");
  Serial.println("Press buttons to test...\n");
  
  Serial.println("════════════════════════════════════════");
  Serial.println("      STARTING DATA STREAM...            ");
  Serial.println("════════════════════════════════════════\n");
}

void loop() {
  static unsigned long motionHelpTime = millis() + 10000;  // Show help after 10 seconds
  static bool helpShown = false;
  static unsigned long lastRead = 0;
  
  // Check buttons
  checkButtons();
  
  // Read at 30 Hz interval (every 33ms) to match sensor ODR
  if (millis() - lastRead >= 33) {
    lastRead = millis();
    
    // Read and display motion data
    int16_t motionVal = sths.readMotion();
    int16_t objTemp = sths.readObjectTemperature();
    float ambientTemp = sths.readAmbientTemperature();
    
    Serial.print("Motion: ");
    Serial.print(motionVal);
    Serial.print(" | ObjTemp: ");
    Serial.print(objTemp);
    Serial.print(" | AmbTemp: ");
    Serial.print(ambientTemp);
    Serial.print("°C");
    
    // Check for motion flag
    if (sths.isMotion()) {
      Serial.print(" 🚶 MOTION!");
      helpShown = true;  // Motion detected, no need for help
    }
    
    Serial.println();
    
    // Show motion detection help if nothing detected for 10 seconds
    if (!helpShown && millis() > motionHelpTime) {
      Serial.println("\n⚠️  MOTION DETECTION TIPS:");
      Serial.println("   - Sensor detects MOVING heat sources");
      Serial.println("   - Wave your hand SLOWLY back and forth");
      Serial.println("   - Stay ~10-30cm from sensor");
      Serial.println("   - Try Button 2 (Sensitivity) lower values");
      Serial.println("   - Or Button 1 (LPF) / Button 3 (Hysteresis)");
      Serial.println();
      helpShown = true;
    }
  }
  
  delay(1);  // Small delay to prevent busy-waiting
}

void checkButtons() {
  static bool button1Pressed = false;
  static bool button2Pressed = false;
  static bool button3Pressed = false;
  
  bool button1Reading = digitalRead(BUTTON1_PIN);
  bool button2Reading = digitalRead(BUTTON2_PIN);
  bool button3Reading = digitalRead(BUTTON3_PIN);
  
  // Button 1 - Cycle LPF
  if (button1Reading == HIGH && !button1Pressed) {
    button1Pressed = true;
    delay(50);  // Simple debounce
    cycleLPF();
  } else if (button1Reading == LOW) {
    button1Pressed = false;
  }
  
  // Button 2 - Cycle Sensitivity
  if (button2Reading == HIGH && !button2Pressed) {
    button2Pressed = true;
    delay(50);  // Simple debounce
    cycleSensitivity();
  } else if (button2Reading == LOW) {
    button2Pressed = false;
  }
  
  // Button 3 - Cycle Hysteresis
  if (button3Reading == HIGH && !button3Pressed) {
    button3Pressed = true;
    delay(50);  // Simple debounce
    cycleHysteresis();
  } else if (button3Reading == LOW) {
    button3Pressed = false;
  }
}

void cycleLPF() {
  lpfIndex = (lpfIndex + 1) % 7;
  if (sths.setMotionLowPassFilter(lpfOptions[lpfIndex])) {
    Serial.println("\n📊 Low-Pass Filter changed:");
    Serial.print("   → ");
    Serial.println(lpfNames[lpfIndex]);
    Serial.println();
    delay(500);  // Pause to see the change
  } else {
    Serial.println("❌ Failed to set LPF");
    delay(500);
  }
}

void cycleSensitivity() {
  sensitivityIndex = (sensitivityIndex + 1) % 9;
  if (sths.setSensitivity(sensitivityLevels[sensitivityIndex])) {
    Serial.println("\n🎚️  Motion Sensitivity changed:");
    Serial.print("   → ");
    Serial.println(sensitivityNames[sensitivityIndex]);
    Serial.println();
    delay(500);  // Pause to see the change
  } else {
    Serial.println("❌ Failed to set Sensitivity");
    delay(500);
  }
}

void cycleHysteresis() {
  hysteresisIndex = (hysteresisIndex + 1) % 9;
  setHysteresis(hysteresisLevels[hysteresisIndex]);
  Serial.println("\n📉 Motion Hysteresis changed:");
  Serial.print("   → ");
  Serial.println(hysteresisNames[hysteresisIndex]);
  Serial.println();
  delay(500);  // Pause to see the change
}

void setHysteresis(uint8_t value) {
  // Write to HYST_MOTION register (0x26) in embedded function page
  sths.enableEmbeddedFuncPage(true);
  sths.writeEmbeddedFunction(0x26, &value, 1);
  sths.enableEmbeddedFuncPage(false);
  
  // Give sensor time to process the change and resume normal operation
  delay(100);
}

void applySettings() {
  Serial.println("Applying initial settings...");
  
  // Disable Block Data Update to allow continuous reading
  Serial.print("Disabling Block Data Update... ");
  if (!sths.setBlockDataUpdate(false)) {
    Serial.println("❌ FAILED");
  } else {
    Serial.println("✅ Success");
  }
  
  // ODR must be set FIRST before other settings
  Serial.print("Setting ODR to 30 Hz... ");
  if (!sths.setOutputDataRate(odrOptions[ODR_30HZ])) {
    Serial.println("❌ FAILED!");
  } else {
    Serial.println("✅ Success");
    // Verify by reading back
    sths34pf80_odr_t currentODR = sths.getOutputDataRate();
    Serial.print("   Verified ODR: ");
    Serial.println(odrNames[currentODR - 1]); // ODR enum starts at 1, not 0
  }
  
  Serial.print("Setting LPF to ");
  Serial.print(lpfNames[lpfIndex]);
  Serial.print("... ");
  if (!sths.setMotionLowPassFilter(lpfOptions[lpfIndex])) {
    Serial.println("❌ FAILED");
  } else {
    Serial.println("✅ Success");
  }
  
  Serial.print("Setting Sensitivity to ");
  Serial.print(sensitivityNames[sensitivityIndex]);
  Serial.print("... ");
  if (!sths.setSensitivity(sensitivityLevels[sensitivityIndex])) {
    Serial.println("❌ FAILED");
  } else {
    Serial.println("✅ Success");
  }
  
  // Set motion hysteresis
  Serial.print("Setting Hysteresis to ");
  Serial.print(hysteresisNames[hysteresisIndex]);
  Serial.print("... ");
  setHysteresis(hysteresisLevels[hysteresisIndex]);
  Serial.println("✅ Success");
  
  Serial.println("✅ All settings applied");
  
  delay(100);
}

void printSettings() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║         CURRENT SETTINGS               ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Low-Pass Filter: ");
  Serial.print(lpfNames[lpfIndex]);
  for(int i = strlen(lpfNames[lpfIndex]); i < 22; i++) Serial.print(" ");
  Serial.println("║");
  Serial.println("║ Output Data Rate: 30 Hz (fixed)       ║");
  Serial.print("║ Sensitivity: ");
  Serial.print(sensitivityNames[sensitivityIndex]);
  for(int i = strlen(sensitivityNames[sensitivityIndex]); i < 26; i++) Serial.print(" ");
  Serial.println("║");
  Serial.print("║ Hysteresis: ");
  Serial.print(hysteresisNames[hysteresisIndex]);
  for(int i = strlen(hysteresisNames[hysteresisIndex]); i < 27; i++) Serial.print(" ");
  Serial.println("║");
  Serial.println("║ Gain Mode: Default                    ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

void printHelp() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║           BUTTON CONTROLS              ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ Button 1 (bottom): Low-Pass Filter    ║");
  Serial.println("║ Button 2 (middle): Motion Sensitivity ║");
  Serial.println("║ Button 3 (top):    Motion Hysteresis  ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  Serial.println("💡 Sensitivity: Lower = more sensitive");
  Serial.println("   Hysteresis: Higher = more stable\n");
}
