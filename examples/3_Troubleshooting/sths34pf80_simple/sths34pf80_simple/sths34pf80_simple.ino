// Advanced STHS34PF80 infrared sensor test with button controls
// Button 1 (GPIO 40): Cycle through Low-Pass Filter settings
// Button 2 (GPIO 39): Cycle through ODR (Output Data Rate) settings

#include "Adafruit_STHS34PF80.h"
#include <Wire.h>

// Define I2C_2 bus with FED4 pins
#define SDA_2 20
#define SCL_2 19

// Button pins (FED4 buttons)
#define BUTTON1_PIN 40  // Bottom button
#define BUTTON2_PIN 39  // Middle button

TwoWire I2C_2 = TwoWire(0);  // Use I2C peripheral 0
Adafruit_STHS34PF80 sths;

// Current settings indices
int lpfIndex = 0;
int odrIndex = 3;  // Start at 1 Hz

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
  
  // Initialize I2C_2 with the correct pins
  I2C_2.begin(SDA_2, SCL_2);
  I2C_2.setClock(400000);  // Set to 400kHz (fast mode)
  
  // Initialize sensor
  if (!sths.begin(0x5A, &I2C_2)) {
    Serial.println("❌ STHS34PF80 NOT FOUND!");
    while(1) delay(100);
  }
  Serial.println("✅ STHS34PF80 Found on I2C_2!\n");
  
  // Apply initial settings
  applySettings();
  
  // Reset the motion detection algorithm
  Serial.println("Resetting motion algorithm...");
  sths.enableEmbeddedFuncPage(true);
  uint8_t reset_val = 0x01;
  sths.writeEmbeddedFunction(0x2A, &reset_val, 1);  // STHS34PF80_EMBEDDED_RESET_ALGO
  sths.enableEmbeddedFuncPage(false);
  
  // Give sensor time to stabilize
  Serial.println("Warming up sensor (3 seconds)...");
  for(int i = 3; i > 0; i--) {
    Serial.print(i);
    Serial.print("... ");
    delay(1000);
  }
  Serial.println("Ready!\n");
  
  printSettings();
  printHelp();
  
  // Debug button state
  Serial.println("\n--- Button Debug Info ---");
  Serial.print("Button 1 (GPIO 40) state: ");
  Serial.println(digitalRead(BUTTON1_PIN) ? "HIGH" : "LOW");
  Serial.print("Button 2 (GPIO 39) state: ");
  Serial.println(digitalRead(BUTTON2_PIN) ? "HIGH" : "LOW");
  Serial.println("Press buttons to test...\n");
}

void loop() {
  static unsigned long motionHelpTime = millis() + 10000;  // Show help after 10 seconds
  static bool helpShown = false;
  
  // Check buttons
  checkButtons();
  
  // Read and display motion data
  if (sths.isDataReady()) {
    // Read motion value
    int16_t motionVal = sths.readMotion();
    int16_t presenceVal = sths.readPresence();
    int16_t objTemp = sths.readObjectTemperature();
    float ambientTemp = sths.readAmbientTemperature();
    
    Serial.print("Motion: ");
    Serial.print(motionVal);
    Serial.print(" | Presence: ");
    Serial.print(presenceVal);
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
    
    // Check for presence flag
    if (sths.isPresence()) {
      Serial.print(" 👤 PRESENCE!");
    }
    
    Serial.println();
    
    // Show motion detection help if nothing detected for 10 seconds
    if (!helpShown && millis() > motionHelpTime) {
      Serial.println("\n⚠️  MOTION DETECTION TIPS:");
      Serial.println("   - Sensor detects MOVING heat sources");
      Serial.println("   - Wave your hand SLOWLY back and forth");
      Serial.println("   - Stay ~10-30cm from sensor");
      Serial.println("   - Try different LPF settings with Button 1");
      Serial.println();
      helpShown = true;
    }
  }
  
  delay(100);
}

void checkButtons() {
  static bool button1Pressed = false;
  static bool button2Pressed = false;
  
  bool button1Reading = digitalRead(BUTTON1_PIN);
  bool button2Reading = digitalRead(BUTTON2_PIN);
  
  // Button 1 - Simple press detection
  if (button1Reading == HIGH && !button1Pressed) {
    button1Pressed = true;
    delay(50);  // Simple debounce
    cycleLPF();
  } else if (button1Reading == LOW) {
    button1Pressed = false;
  }
  
  // Button 2 - Simple press detection
  if (button2Reading == HIGH && !button2Pressed) {
    button2Pressed = true;
    delay(50);  // Simple debounce
    cycleODR();
  } else if (button2Reading == LOW) {
    button2Pressed = false;
  }
}

void cycleLPF() {
  lpfIndex = (lpfIndex + 1) % 7;
  if (sths.setMotionLowPassFilter(lpfOptions[lpfIndex])) {
    Serial.println("\n📊 Low-Pass Filter changed:");
    Serial.print("   → ");
    Serial.println(lpfNames[lpfIndex]);
  } else {
    Serial.println("❌ Failed to set LPF");
  }
}

void cycleODR() {
  odrIndex = (odrIndex + 1) % 8;
  if (sths.setOutputDataRate(odrOptions[odrIndex])) {
    Serial.println("\n⏱️  Output Data Rate changed:");
    Serial.print("   → ");
    Serial.println(odrNames[odrIndex]);
  } else {
    Serial.println("❌ Failed to set ODR");
  }
}

void applySettings() {
  Serial.println("Applying initial settings...");
  
  if (!sths.setMotionLowPassFilter(lpfOptions[lpfIndex])) {
    Serial.println("❌ Failed to set initial LPF");
  }
  
  if (!sths.setOutputDataRate(odrOptions[odrIndex])) {
    Serial.println("❌ Failed to set initial ODR");
  }
  
  // Use default gain mode (no need to set explicitly)
  Serial.println("✅ Using default gain mode");
  
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
  Serial.print("║ Output Data Rate: ");
  Serial.print(odrNames[odrIndex]);
  for(int i = strlen(odrNames[odrIndex]); i < 21; i++) Serial.print(" ");
  Serial.println("║");
  Serial.println("║ Gain Mode: Default                    ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

void printHelp() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║           BUTTON CONTROLS              ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ Button 1 (bottom): Low-Pass Filter    ║");
  Serial.println("║ Button 2 (middle): Output Data Rate   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}
