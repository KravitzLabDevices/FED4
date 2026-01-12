/*
  FED4 Accelerometer Test
  
  This program tests the LIS3DH accelerometer on the FED4 device.
  It continuously reads acceleration data on X, Y, and Z axes and displays
  the values on the screen in both m/s² and g units.
  
  DEVICE AXIS ORIENTATION (when screen is facing you):
  - X axis: Left/Right across the screen
  - Y axis: Forward/Backward (depth, into/out of screen)
  - Z axis: Up/Down (perpendicular to screen)
  
  When stationary and lying flat, Z should read approximately 9.8 m/s² (1g) due to gravity.
  
  Hardware: FED4 with LIS3DH accelerometer at I2C address 0x19
*/

#include <FED4.h>

FED4 fed;

// Function declarations
void updateDisplay();
void drawBall(int x, int y, int radius);
void drawBallWhite(int x, int y, int radius);

// Variables to store acceleration data
float accelX = 0.0;
float accelY = 0.0;
float accelZ = 0.0;

// Update interval
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 100;  // Update every 100ms

// Ball physics
float ballX = 72.0;  // Center X (screen width 144)
float ballY = 84.0;  // Center Y (screen height 168)
float ballVelX = 0.0;
float ballVelY = 0.0;
int oldBallX = 72;  // Previous ball position for erasing
int oldBallY = 84;
const float ballRadius = 4.0;
const float damping = 0.85;     // Energy loss on collision (increased for less bouncy)
const float friction = 0.92;    // Velocity decay (increased friction)
const float sensitivity = 1.5;  // Accelerometer sensitivity multiplier

// Calibration - store initial orientation to subtract gravity
float gravityX = 0.0;
float gravityY = 0.0;
float gravityZ = 0.0;
bool calibrated = false;

bool firstDraw = true;  // Flag to track if this is the first draw

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("FED4 Accelerometer Test");
  Serial.println("=======================");

  // Initialize FED4
  if (!fed.begin("Accel_Test")) {
    Serial.println("ERROR: Failed to initialize FED4");
    while (1) delay(1000);
  }

  Serial.println("FED4 initialized");

  // Initialize accelerometer
  if (!fed.initializeAccel()) {
    Serial.println("ERROR: Failed to initialize accelerometer");

    // Display error on screen
    fed.clearDisplay();
    fed.setFont(&FreeSans9pt7b);
    fed.setTextSize(1);
    fed.setTextColor(DISPLAY_BLACK);
    fed.setCursor(10, 30);
    fed.print("Accelerometer");
    fed.setCursor(10, 50);
    fed.print("Init Failed!");
    fed.setCursor(10, 80);
    fed.print("Check I2C");
    fed.setCursor(10, 100);
    fed.print("Connection");
    fed.refresh();

    while (1) delay(1000);
  }

  Serial.println("Accelerometer initialized successfully");

  // Configure accelerometer
  fed.setAccelRange(LIS3DH_RANGE_2_G);                       // ±2g range
  fed.setAccelDataRate(LIS3DH_DATARATE_50_HZ);               // 50 Hz sample rate
  fed.setAccelPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);  // 12-bit resolution

  Serial.println("Configuration:");
  Serial.println("  Range: ±2g");
  Serial.println("  Data Rate: 50 Hz");
  Serial.println("  Mode: High Resolution (12-bit)");
  Serial.println("");

  // Calibrate gravity offset
  Serial.println("Calibrating... Keep device still!");
  delay(500);

  // Take average of several readings for calibration
  float sumX = 0, sumY = 0, sumZ = 0;
  const int numSamples = 20;
  for (int i = 0; i < numSamples; i++) {
    float x, y, z;
    fed.readAccel(x, y, z);
    sumX += x;
    sumY += y;
    sumZ += z;
    delay(50);
  }

  gravityX = sumX / numSamples;
  gravityY = sumY / numSamples;
  gravityZ = sumZ / numSamples;
  calibrated = true;

  Serial.print("Gravity offset calibrated - X(L/R):");
  Serial.print(gravityX, 2);
  Serial.print(" Y:");
  Serial.print(gravityY, 2);
  Serial.print(" Z:");
  Serial.println(gravityZ, 2);
  Serial.println("");
  Serial.println("Device Axis Orientation:");
  Serial.println("  X = Left/Right");
  Serial.println("  Y = Forward/Back");
  Serial.println("  Z = Up/Down");
  Serial.println("");
  Serial.println("Starting continuous readings...");
  Serial.println("Tilt device to move the ball!");
  Serial.println("");

  // Initial display setup
  updateDisplay();
}

void loop() {
  // Check if it's time to update
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();

    // Read accelerometer data
    fed.readAccel(accelX, accelY, accelZ);

    // Print to Serial (device orientation)
    Serial.print("X: ");
    Serial.print(accelX, 3);
    Serial.print(" m/s² (");
    Serial.print(accelX / 9.81, 3);
    Serial.print("g)\t");

    Serial.print("Y: ");
    Serial.print(accelY, 3);
    Serial.print(" m/s² (");
    Serial.print(accelY / 9.81, 3);
    Serial.print("g)\t");

    Serial.print("Z: ");
    Serial.print(accelZ, 3);
    Serial.print(" m/s² (");
    Serial.print(accelZ / 9.81, 3);
    Serial.println("g)");

    // Update display
    updateDisplay();
  }
}

void updateDisplay() {
  // Set font and text properties
  fed.setFont(&FreeSans9pt7b);
  fed.setTextSize(1);
  fed.setTextColor(DISPLAY_BLACK);

  // Draw static elements only on first draw
  if (firstDraw) {
    fed.clearDisplay();

    // Title
    fed.setCursor(10, 20);
    fed.print("Accelerometer");

    // Draw separator line
    fed.drawLine(0, 30, 168, 30, DISPLAY_BLACK);

    firstDraw = false;
  }

  // Draw labels (device orientation)
  fed.setCursor(5, 50);
  fed.print("X: ");
  fed.setCursor(5, 75);
  fed.print("Y: ");
  fed.setCursor(5, 100);
  fed.print("Z: ");

  // Clear only the dynamic value areas (not the labels)
  fed.fillRect(25, 35, 90, 75, DISPLAY_WHITE);    // Clear X, Y, Z value areas

  // Display X axis value (Left/Right)
  fed.setCursor(30, 50);
  fed.print(accelX, 1);

  // Display Y axis value (Forward/Back)
  fed.setCursor(30, 75);
  fed.print(accelY, 1);

  // Display Z axis value (Up/Down)
  fed.setCursor(30, 100);
  fed.print(accelZ, 1);


  // Erase old ball position
  drawBallWhite(oldBallX, oldBallY, (int)ballRadius);

  // Update ball physics based on accelerometer
  // Subtract gravity offset to get only the tilt acceleration
  float tiltX = calibrated ? (accelX - gravityX) : 0;
  float tiltY = calibrated ? (accelY - gravityY) : 0;
  float tiltZ = calibrated ? (accelZ - gravityZ) : 0;

  // Device axes are now correctly oriented:
  // X = left/right, Y = forward/back (unused), Z = up/down
  // Note: Display coordinates have Y increasing downward
  float dt = updateInterval / 1000.0;  // Convert to seconds

  // Apply acceleration to velocity (X controls horizontal, Z controls vertical)
  ballVelX += tiltX * sensitivity * dt;  // X acceleration for left/right
  ballVelY -= tiltZ * sensitivity * dt;  // Z acceleration for up/down (negative because screen Y is inverted)

  // Apply friction
  ballVelX *= friction;
  ballVelY *= friction;

  // Store old position for erasing
  oldBallX = (int)ballX;
  oldBallY = (int)ballY;

  // Update position
  ballX += ballVelX;
  ballY += ballVelY;

  // Boundary collision detection with damping
  // Screen is 144 pixels wide (X) and 168 pixels tall (Y)
  if (ballX - ballRadius < 0) {
    ballX = ballRadius;
    ballVelX = -ballVelX * damping;
  }
  if (ballX + ballRadius > 144) {  // Fixed: screen width is 144, not 168
    ballX = 144 - ballRadius;
    ballVelX = -ballVelX * damping;
  }
  if (ballY - ballRadius < 32) {  // Top boundary below the separator line
    ballY = 32 + ballRadius;
    ballVelY = -ballVelY * damping;
  }
  if (ballY + ballRadius > 168) {
    ballY = 168 - ballRadius;
    ballVelY = -ballVelY * damping;
  }

  // Draw the ball
  drawBall((int)ballX, (int)ballY, (int)ballRadius);

  // Refresh the display
  fed.refresh();
}

void drawBall(int x, int y, int radius) {
  // Draw a filled circle for the ball
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        int px = x + dx;
        int py = y + dy;
        if (px >= 0 && px < 144 && py >= 0 && py < 168) {  // Screen is 144x168
          fed.drawPixel(px, py, DISPLAY_BLACK);
        }
      }
    }
  }
}

void drawBallWhite(int x, int y, int radius) {
  // Erase a filled circle (draw in white)
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        int px = x + dx;
        int py = y + dy;
        if (px >= 0 && px < 144 && py >= 0 && py < 168) {  // Screen is 144x168
          fed.drawPixel(px, py, DISPLAY_WHITE);
        }
      }
    }
  }
}