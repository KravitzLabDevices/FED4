/*
 * FED4 Deep Sleep Test (updated hardware)
 *
 * Triggers sleep from:
 * - Buttons (15,16,39)
 * - Touch pads (NUM1/NUM2/NUM3)
 * - Center photogate (GPIO 14, active LOW)
 *
 * Wakes from:
 * - INT_OR pin (GPIO 7, active LOW)
 * - Timer (default 10s, optional backup)
 * - Touchpad wake
 *
 * Notes:
 * - Single primary I2C bus only (SDA=8, SCL=9)
 * - Power rails are enabled through MCP23017:
 *   EXP_PSV2_EN=13, EXP_PSV3_EN=12
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// Primary I2C
#define SDA_PIN 8
#define SCL_PIN 9

// MCP power rails
#define EXP_PSV2_EN 13
#define EXP_PSV3_EN 12

// Inputs
#define BUTTON_1_PIN 15
#define BUTTON_2_PIN 16
#define BUTTON_3_PIN 39
#define PHOTOGATE_1_PIN 14
#define INT_OR_PIN 7

// Touch pads (same mapping as src/FED4_Pins.h)
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM3
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM2
#define TOUCH_PAD_LEFT TOUCH_PAD_NUM1

RTC_DATA_ATTR int bootCount = 0;

Adafruit_MCP23X17 mcp;

uint16_t baseLeft = 0;
uint16_t baseCenter = 0;
uint16_t baseRight = 0;
static constexpr float TOUCH_THRESHOLD = 0.20f;
static constexpr uint64_t SLEEP_SECONDS = 10;

void printWakeReason() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  switch (reason) {
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wake: timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wake: touchpad"); break;
    case ESP_SLEEP_WAKEUP_GPIO:     Serial.println("Wake: gpio"); break;
    default:                        Serial.printf("Wake: other (%d)\n", reason); break;
  }
}

void calibrateTouch() {
  delay(30);
  baseLeft = touchRead(TOUCH_PAD_LEFT);
  baseCenter = touchRead(TOUCH_PAD_CENTER);
  baseRight = touchRead(TOUCH_PAD_RIGHT);
  Serial.printf("Touch baseline L:%u C:%u R:%u\n", baseLeft, baseCenter, baseRight);
}

bool touched(uint16_t current, uint16_t baseline) {
  if (baseline == 0) return false;
  float dev = fabs((float)current / (float)baseline - 1.0f);
  return dev >= TOUCH_THRESHOLD;
}

void configureWakeSources() {
  // Timer wake for deterministic wake cycles.
  esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);

  // Deep-sleep wake on INT_OR (active-LOW OR of interrupt sources).
  esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_OR_PIN, 0);

  // Touch wake.
  esp_sleep_enable_touchpad_wakeup();
}

void enterDeepSleep(const char *cause) {
  Serial.printf("Sleep trigger: %s\n", cause);
  Serial.printf("Entering deep sleep for %llu second(s)...\n", SLEEP_SECONDS);
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  bootCount++;
  Serial.printf("\n=== FED4 Deep Sleep Test | Boot %d ===\n", bootCount);
  printWakeReason();

  // Current hardware uses only main I2C.
  Wire.begin(SDA_PIN, SCL_PIN, 100000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 init failed");
    while (1) delay(10);
  }

  // Enable rails used by sensors/LED strip.
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);

  // Direct GPIO inputs on current board.
  pinMode(BUTTON_1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_2_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_3_PIN, INPUT_PULLDOWN);
  pinMode(PHOTOGATE_1_PIN, INPUT_PULLUP); // active LOW when blocked
  pinMode(INT_OR_PIN, INPUT); // external pull-up on hardware

  calibrateTouch();
  configureWakeSources();

  Serial.println("Ready: sleep trigger = button/touch/photogate.");
  Serial.println("Wake sources: INT_OR LOW, touchpad, or timer.");
}

void loop() {
  bool b1 = digitalRead(BUTTON_1_PIN);
  bool b2 = digitalRead(BUTTON_2_PIN);
  bool b3 = digitalRead(BUTTON_3_PIN);
  bool pg1Blocked = (digitalRead(PHOTOGATE_1_PIN) == LOW);

  bool tLeft = touched(touchRead(TOUCH_PAD_LEFT), baseLeft);
  bool tCenter = touched(touchRead(TOUCH_PAD_CENTER), baseCenter);
  bool tRight = touched(touchRead(TOUCH_PAD_RIGHT), baseRight);

  if (b1) enterDeepSleep("BUTTON_1");
  if (b2) enterDeepSleep("BUTTON_2");
  if (b3) enterDeepSleep("BUTTON_3");
  if (pg1Blocked) enterDeepSleep("PHOTOGATE_1");
  if (tLeft) enterDeepSleep("TOUCH_LEFT");
  if (tCenter) enterDeepSleep("TOUCH_CENTER");
  if (tRight) enterDeepSleep("TOUCH_RIGHT");

  delay(20);
}