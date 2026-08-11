/*
 * FED4 Touch Light Sleep (ESP32-S3)
 *
 * Three touch pads wake from light sleep (native touchpad wake, not EXT1 GPIO).
 * Minimal display + Serial feedback. Panel image held during light sleep via
 * VCOM keepalive between esp_light_sleep timer chunks.
 * RGB strip shows proportional touch response on wake (Serial drops in sleep).
 *
 * Touch pads (FED4_Pins.h):
 *   LEFT   = TOUCH_PAD_NUM1 (GPIO 1)
 *   CENTER = TOUCH_PAD_NUM3 (GPIO 3)
 *   RIGHT  = TOUCH_PAD_NUM2 (GPIO 2)
 *
 * ESP32-S3 touch notes (see FED4_TouchS3 / FED4-Touch.ino):
 *   - Counts RISE when touched; values are uint32_t (not uint16_t).
 *   - Boot idle baseline + rise fraction for wake identification.
 *   - IDF 5.5+: NG direct touch_sens driver (FED4_TouchS3).
 *
 * Flash with Tools -> "USB CDC On Boot" = ENABLED (GPIO 43/44 are display pins).
 * Serial optional: 500 ms grace delay, then runs without a monitor attached.
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>
#include <FED4_Pins.h>
#include <FED4_DisplayOrient.h>
#include "../FED4_TouchS3/FED4_TouchS3.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
  {                         \
    int16_t t = a;          \
    a = b;                  \
    b = t;                  \
  }
#endif

static const uint16_t PANEL_WIDTH = 320;
static const uint16_t PANEL_HEIGHT = 176;
static const uint8_t PIXEL_BLACK = 0;
static const uint8_t PIXEL_WHITE = 1;
static const uint32_t SPI_HZ = 1000000;

static const uint32_t VCOM_MS = 500;
static const uint32_t SERIAL_BOOT_DELAY_MS = 1000;
static const uint32_t STRIP_MS = 8;
static const uint32_t TOUCH_LED_MIN_MS = 400;
static const uint32_t TOUCH_LED_MAX_MS = 3000;

// S3 touch tuning (FED4_TouchS3)
static const float TOUCH_TRIGGER_RISE = 0.03f;  // wake threshold — 3% rise above idle
// Strip brightness mapping (FED4-Demo-Hardware.ino)
static const float TOUCH_LED_FULL_RISE = 0.05f;
static const float TOUCH_DEADBAND = 0.015f;
static const uint8_t TOUCH_LED_MIN_BRIGHT = 28;
static const uint8_t STRIP_BRIGHTNESS = 120;

#define NUM_STRIP_LEDS 8

static const uint8_t PROGMEM set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                             clr[] = {(uint8_t)~1,   (uint8_t)~2,   (uint8_t)~4,
                                      (uint8_t)~8,   (uint8_t)~16,  (uint8_t)~32,
                                      (uint8_t)~64,  (uint8_t)~128};

Adafruit_MCP23X17 mcp;
Adafruit_NeoPixel strip(NUM_STRIP_LEDS, RGB_STRIP, NEO_GRB + NEO_KHZ800);

uint32_t wakeCount = 0;

// ---------------------------------------------------------------------------
// MIP display (minimal, from FED4-SleepModes / FED4-Display-Standalone)
// ---------------------------------------------------------------------------

class MIPDisplay : public Adafruit_GFX {
public:
  MIPDisplay() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

  bool begin() {
    const uint32_t bufferSize = (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8;
    if (frameBuffer) {
      free(frameBuffer);
      frameBuffer = nullptr;
    }
    frameBuffer = (uint8_t *)malloc(bufferSize);
    if (!frameBuffer) return false;
    memset(frameBuffer, 0x00, bufferSize);
    setRotation(FED4_DISPLAY_ROTATION_NATIVE);
    return true;
  }

  void clearBlack() {
    memset(frameBuffer, 0x00, (uint32_t)PANEL_WIDTH * PANEL_HEIGHT / 8);
  }

  void refresh() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
    delay(4);
    delayMicroseconds(30);
    digitalWrite(DISPLAY_CS, HIGH);
    delay(5);

    SPI.beginTransaction(SPISettings(SPI_HZ, LSBFIRST, SPI_MODE0));
    const uint8_t bytesPerLine = PANEL_WIDTH / 8;
    for (uint8_t line = 0; line < PANEL_HEIGHT; line++) {
      SPI.transfer(line);
      const uint8_t *row = frameBuffer + (uint32_t)line * bytesPerLine;
      for (uint8_t b = 0; b < bytesPerLine; b++) SPI.transfer(row[b]);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
      SPI.transfer(0x00);
    }
    SPI.endTransaction();
    delay(2);
    digitalWrite(DISPLAY_CS, LOW);
  }

  void toggleVcomKeepAlive() {
    vcomState = !vcomState;
    digitalWrite(DISPLAY_VCOM, vcomState ? HIGH : LOW);
  }

  void setVcomLow() {
    vcomState = false;
    digitalWrite(DISPLAY_VCOM, LOW);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

    int16_t px = x, py = y;
    switch (rotation) {
      case 1:
        _swap_int16_t(px, py);
        px = PANEL_WIDTH - 1 - px;
        break;
      case 2:
        px = PANEL_WIDTH - 1 - px;
        py = PANEL_HEIGHT - 1 - py;
        break;
      case 3:
        _swap_int16_t(px, py);
        py = PANEL_HEIGHT - 1 - py;
        break;
      default:
        break;
    }

    if (color)
      frameBuffer[(py * PANEL_WIDTH + px) / 8] |= pgm_read_byte(&set[px & 7]);
    else
      frameBuffer[(py * PANEL_WIDTH + px) / 8] &= pgm_read_byte(&clr[px & 7]);
  }

private:
  uint8_t *frameBuffer = nullptr;
  bool vcomState = false;
};

MIPDisplay display;

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void displayReset() {
  mcp.pinMode(EXP_DISPLAY_RESET, OUTPUT);
  pinMode(DISPLAY_VCOM, OUTPUT);
  display.setVcomLow();
  mcp.digitalWrite(EXP_DISPLAY_RESET, HIGH);
  delay(10);
  mcp.digitalWrite(EXP_DISPLAY_RESET, LOW);
  delay(10);
}

void displayLight(bool on) {
  mcp.pinMode(EXP_DISPLAY_LED, OUTPUT);
  mcp.digitalWrite(EXP_DISPLAY_LED, on ? HIGH : LOW);
}

void drawScreen(const char *status, const char *detail) {
  display.clearBlack();
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(PIXEL_WHITE);
  display.setCursor(8, 20);
  display.print("Touch Light Sleep");
  display.setCursor(8, 50);
  display.print(status);
  if (detail && detail[0]) {
    display.setCursor(8, 70);
    display.print(detail);
  }
  display.setCursor(8, 100);
  display.printf("Wakes: %lu", (unsigned long)wakeCount);
  display.refresh();
}

// ---------------------------------------------------------------------------
// RGB strip — proportional touch glow (FED4-Demo-Hardware.ino)
// 0-2 right (blue), 3-4 center (green), 5-7 left (red)
// ---------------------------------------------------------------------------

uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  return strip.Color(r * bright / 255, g * bright / 255, b * bright / 255);
}

uint8_t touchRiseToBrightness(uint32_t raw, uint32_t idle) {
  if (!idle || raw <= idle) return 0;

  const float rise = (float)(raw - idle) / (float)idle;
  if (rise < TOUCH_DEADBAND) return 0;

  const float span = TOUCH_LED_FULL_RISE - TOUCH_DEADBAND;
  float t = (rise - TOUCH_DEADBAND) / span;
  if (t > 1.0f) t = 1.0f;
  t = powf(t, 0.75f);

  uint8_t bright = (uint8_t)(t * 255.0f);
  if (bright > 0 && bright < TOUCH_LED_MIN_BRIGHT)
    bright = TOUCH_LED_MIN_BRIGHT;
  return bright;
}

void updateTouchStrip() {
  const uint32_t l = fed4TouchS3Read(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchS3Read(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchS3Read(TOUCH_PAD_RIGHT);

  const uint8_t brightL = touchRiseToBrightness(l, fed4TouchIdleL);
  const uint8_t brightC = touchRiseToBrightness(c, fed4TouchIdleC);
  const uint8_t brightR = touchRiseToBrightness(r, fed4TouchIdleR);

  for (int i = 0; i < 3; i++)
    strip.setPixelColor(i, scaleColor(0, 0, 255, brightR));
  for (int i = 3; i < 5; i++)
    strip.setPixelColor(i, scaleColor(0, 255, 0, brightC));
  for (int i = 5; i < NUM_STRIP_LEDS; i++)
    strip.setPixelColor(i, scaleColor(255, 0, 0, brightL));
  strip.show();
}

bool anyStripTouchActive() {
  const uint32_t l = fed4TouchS3Read(TOUCH_PAD_LEFT);
  const uint32_t c = fed4TouchS3Read(TOUCH_PAD_CENTER);
  const uint32_t r = fed4TouchS3Read(TOUCH_PAD_RIGHT);
  return touchRiseToBrightness(l, fed4TouchIdleL) ||
         touchRiseToBrightness(c, fed4TouchIdleC) ||
         touchRiseToBrightness(r, fed4TouchIdleR);
}

// Poll strip while pads are held; min/max window for sensitivity checks.
void runTouchLedFeedback() {
  const unsigned long startMs = millis();
  unsigned long lastTouchMs = startMs;

  while (true) {
    updateTouchStrip();

    if (anyStripTouchActive())
      lastTouchMs = millis();

    const unsigned long now = millis();
    if (now - startMs >= TOUCH_LED_MAX_MS) break;
    if (now - lastTouchMs >= TOUCH_LED_MIN_MS && now - startMs >= TOUCH_LED_MIN_MS) break;

    delay(STRIP_MS);
  }

  strip.clear();
  strip.show();
}

// Light sleep until touch; timer chunks keep MIP VCOM alive (FED4-SleepModes).
void lightSleepUntilTouch() {
  while (true) {
    display.toggleVcomKeepAlive();
    esp_sleep_enable_timer_wakeup((uint64_t)VCOM_MS * 1000ULL);
    Serial.flush();
    esp_light_sleep_start();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TOUCHPAD) break;
  }
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(SERIAL_BOOT_DELAY_MS);
  Serial.println("Serial ready.");

  Wire.begin(SDA, SCL, 100000);
  Wire.setTimeout(1000);

  if (!mcp.begin_I2C()) {
    Serial.println("MCP23017 fail");
    while (1) delay(10);
  }

  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  mcp.digitalWrite(EXP_PSV2_EN, LOW);
  mcp.digitalWrite(EXP_PSV3_EN, LOW);
  delay(5);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  displayReset();
  displayLight(false);

  if (!display.begin()) {
    Serial.println("Display fail");
    while (1) delay(10);
  }

  if (!fed4TouchS3InitPads()) {
    Serial.println("Touch fail — keep pads clear at boot");
    while (1) delay(10);
  }
  if (!fed4TouchS3EnableTouchpadWakeup()) {
    Serial.println("Touch wake fail");
    while (1) delay(10);
  }

  strip.begin();
  strip.setBrightness(STRIP_BRIGHTNESS);
  strip.clear();
  strip.show();

  Serial.printf("Ready idle L:%lu C:%lu R:%lu\n",
                (unsigned long)fed4TouchIdleL, (unsigned long)fed4TouchIdleC,
                (unsigned long)fed4TouchIdleR);
  fed4TouchS3PrintDriverConfig();
}

void loop() {
  strip.clear();
  strip.show();
  drawScreen("Sleeping", "Touch any pad");
  lightSleepUntilTouch();

  wakeCount++;
  const char *pad = fed4TouchS3IdentifyWakePad(TOUCH_TRIGGER_RISE);
  if (pad) {
    Serial.printf("Wake: %s\n", pad);
    drawScreen("Wake", pad);
  } else {
    Serial.println("Wake: touch");
    drawScreen("Wake", "?");
  }

  runTouchLedFeedback();
}
