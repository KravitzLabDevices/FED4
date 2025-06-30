#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep() {
  // Enable timer-based wake-up every N seconds
  int sleepSeconds = 6; //how many seconds to sleep between timer based wake-ups
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000); // Convert 6 seconds to microseconds
  noPix(); 
  startSleep();
  wakeUp();
  redPix(1); //very dim red pix to indicate when FED4 is awake

  // Check sensors on timer wake-up and buttons
  handleTouch();
  checkButton1();
  checkButton2();
  checkButton3();
  pollSensors();
}

// Prepares device for sleep mode by disabling components and entering light sleep
void FED4::startSleep() {
  // Wait for all touch pads to be released before sleeping
  while (true) {
    float leftDev = abs((float)touchRead(TOUCH_PAD_LEFT) / touchPadLeftBaseline - 1.0);
    float centerDev = abs((float)touchRead(TOUCH_PAD_CENTER) / touchPadCenterBaseline - 1.0);
    float rightDev = abs((float)touchRead(TOUCH_PAD_RIGHT) / touchPadRightBaseline - 1.0);
    
    if (leftDev < TOUCH_THRESHOLD && centerDev < TOUCH_THRESHOLD && rightDev < TOUCH_THRESHOLD) {
      break;
    }
    delay(1);
  }

  // Calibrate touch sensors before sleep on every N wake-ups
  if (wakeCount % 10 == 0 )  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    
    // Add delay and I2C recovery after touch calibration to prevent I2C bus issues
    delay(1);  // Give I2C bus time to stabilize (reduced from 100ms)
    I2C_2.begin(SDA_2, SCL_2);  // Reinitialize secondary I2C bus
  }

  // Reset all touch flags before going to sleep
  resetTouchFlags();

  Serial.flush();
  clearStrip();
  noPix();  // Turn off the LED when going to sleep
  LDO3_OFF();  // Turn off LDO3 to power down NeoPixel
  LDO2_OFF();
  enableAmp(false);
  esp_light_sleep_start();
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  wakeCount++;
  LDO2_ON();
  Wire.begin();  // Reinitialize primary I2C
  I2C_2.begin(SDA_2, SCL_2);  // Reinitialize secondary I2C for light sensor
  
  // Reconfigure light sensor after every I2C bus reinitialization
  delay(1);  // Brief delay for bus stabilization
  lightSensor.setGain(VEML7700_GAIN_1_8);
  lightSensor.setIntegrationTime(VEML7700_IT_100MS);
  lightSensor.enable(true);
  
  // Reconfigure motion sensor after I2C bus reinitialization
  if (motionSensor.begin(0x5A, I2C_2)) {
    // Reconfigure motion sensor settings
    motionSensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz);
    motionSensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE);
    motionSensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_20);
    motionSensor.setMotionThreshold(200);
    motionSensor.setMotionHysteresis(10);
  }
  
  // Remove redundant MCP reinitialization - it should already be working
  // mcp.begin_I2C();  // Reinitialize MCP after I2C
  
  // Reconfigure GPIO expander pins after wake-up
  mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);
  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);
  mcp.pinMode(EXP_LDO3, OUTPUT);
  
  LDO3_ON();  // Turn on LDO3 to power up NeoPixel
  enableAmp(true);
}

// Handles touch inputs and only checks them if a button was not pressed
void FED4::handleTouch() {
  pinMode(BUTTON_1, INPUT_PULLDOWN);
  pinMode(BUTTON_2, INPUT_PULLDOWN);
  pinMode(BUTTON_3, INPUT_PULLDOWN);

  // Check if wake-up was caused by timer
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    // This is a timer wake-up, skip touch interpretation
    Serial.print("Timer wake-up");
    wakePad = 0;  // Reset wake pad for timer wake-up
    touch_pad_clear_status();
    return;
  }

  // Check if any buttons are pressed
  if (digitalRead(BUTTON_1) == 1 || digitalRead(BUTTON_2) == 1 || digitalRead(BUTTON_3) == 1) {
    // This is a button wake-up, skip touch interpretation
    Serial.println("Button wake-up");
    wakePad = 0;  // Reset wake pad for button wake-up
    touch_pad_clear_status();
    return;
  }

  // If we get here, this is a touch pad wake-up, so interpret the touch
  interpretTouch();
}

// Initializes LDO (Low-Dropout Regulator) power control pins
bool FED4::initializeLDOs()
{
    mcp.pinMode(EXP_LDO3, OUTPUT);
    LDO3_ON();
    return true;
}

// Enables LDO2 power rail
void FED4::LDO2_ON()
{
    digitalWrite(LDO2_ENABLE, HIGH);
    delayMicroseconds(100); // Minimum 50us stabilization time
}

// Disables LDO2 power rail
void FED4::LDO2_OFF()
{
    digitalWrite(LDO2_ENABLE, LOW);
}

// Enables LDO3 power rail
void FED4::LDO3_ON()
{
    mcp.digitalWrite(EXP_LDO3, HIGH);
    delayMicroseconds(100); // Minimum 50us stabilization time
}

// Disables LDO3 power rail
void FED4::LDO3_OFF()
{
    mcp.digitalWrite(EXP_LDO3, LOW);
} 