#include "FED4.h"

#include "driver/gpio.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep(int seconds)
{
  sleepSeconds = seconds;
  noPix();
  startSleep();
  wakeUp();
}

// For backward compatibility, keep the parameterless version
void FED4::sleep()
{
  sleep(sleepSeconds);
}

// Shared SPI: SD lives on PSV2. With VCC off, any line held HIGH back-powers the
// card through ESD diodes (~16 mA with card inserted). Idle CS is HIGH only while
// powered; for rail-off drive CS/SCK/MOSI/MISO all LOW (not SD_CS HIGH).
static void holdSdSpiPinsLow(bool hold)
{
  const gpio_num_t pins[] = {
      (gpio_num_t)SD_CS,
      (gpio_num_t)SPI_SCK,
      (gpio_num_t)SPI_MOSI,
      (gpio_num_t)SPI_MISO,
  };
  for (gpio_num_t p : pins)
  {
    if (hold)
    {
      gpio_sleep_sel_dis(p);
      gpio_hold_en(p);
    }
    else
    {
      gpio_hold_dis(p);
    }
  }
}

static void quiesceSpiForPsv2Off()
{
  // GO_IDLE while card still has VCC, then release the bus
  SD.end();
  SPI.end();

  pinMode(SD_CS, OUTPUT);
  pinMode(SPI_SCK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_MISO, OUTPUT);
  digitalWrite(SD_CS, LOW);
  digitalWrite(SPI_SCK, LOW);
  digitalWrite(SPI_MOSI, LOW);
  digitalWrite(SPI_MISO, LOW);

  // MIP SCS inactive = LOW (panel on always-on 3.3 V; VCOM is separate LEDC)
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);

  holdSdSpiPinsLow(true);
}

// Drive ESP32 pins into the PSV2 domain LOW before rail off (avoid back-power).
static void quiescePsv2Gpios()
{
  quiesceSpiForPsv2Off();

  pinMode(PHOTOGATE_1, OUTPUT);
  pinMode(PHOTOGATE_2, OUTPUT);
  pinMode(PHOTOGATE_3, OUTPUT);
  pinMode(PHOTOGATE_4, OUTPUT);
  digitalWrite(PHOTOGATE_1, LOW);
  digitalWrite(PHOTOGATE_2, LOW);
  digitalWrite(PHOTOGATE_3, LOW);
  digitalWrite(PHOTOGATE_4, LOW);

  pinMode(AMP_BCLK, OUTPUT);
  pinMode(AMP_LRCLK, OUTPUT);
  pinMode(AMP_DIN, OUTPUT);
  digitalWrite(AMP_BCLK, LOW);
  digitalWrite(AMP_LRCLK, LOW);
  digitalWrite(AMP_DIN, LOW);
}

static void restorePhotogateInputs()
{
  pinMode(PHOTOGATE_1, INPUT_PULLUP);
  pinMode(PHOTOGATE_2, INPUT_PULLUP);
  pinMode(PHOTOGATE_3, INPUT_PULLUP);
  pinMode(PHOTOGATE_4, INPUT_PULLUP);
}

static void restoreSpiAfterPsv2On()
{
  holdSdSpiPinsLow(false);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // powered idle: CS deasserted
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
}

// Light sleep with LEDC VCOM keepalive (FED4-VCOM-LEDC-Light-Sleep):
//   - one timer wake for the full sleepSeconds budget
//   - touchpad + button GPIO wake (INT_OR off); PSV2/PSV3 off for battery life
//   - late retrieval: checkLateRetrieval() on wake (no photogate wake — PSV2 off)
void FED4::startSleep()
{
  lastWakeSource = FedWakeSource::None;

  // Wait for all touch pads to be released before sleeping
  while (!fed4TouchPadsReleased(TOUCH_THRESHOLD))
  {
    delay(1);
  }

  // Rare rebaseline (skip wakeCount==0 — first sleep already calibrated at begin)
  if (wakeCount > 0 && wakeCount % 200 == 0)
  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    delay(1);
  }

  // Clear poke latches and push to MIP — panel retains pixels until refresh
  resetTouchFlags();
  wakePad = 0;
  if (displayBuffer != nullptr)
  {
    displayIndicators();
    refresh();
  }

  noPix();
  enableAmp(false);

  if (sleepyLEDs)
  {
    lightsOff();
    PSV3_OFF();
  }

  // Cut 3.3V2 — photogate IR LEDs / amp domain off (battery); late retrieval is coarse
  quiescePsv2Gpios();
  PSV2_OFF();

  if (sleepSeconds <= 0)
  {
    lastWakeSource = FedWakeSource::Timer;
    return;
  }

  if (interruptPending())
  {
    printInterruptStatus("pre-sleep");
  }

  // INT_OR must not wake us (held-low spam). Buttons may wake via GPIO.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  gpio_wakeup_disable((gpio_num_t)INT_OR);
  gpio_wakeup_disable((gpio_num_t)PHOTOGATE_1);
  gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_2, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_3, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  fed4TouchEnableTouchpadWakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

  Serial.flush();
  esp_light_sleep_start();

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool buttonHigh = digitalRead(BUTTON_1) == HIGH ||
                          digitalRead(BUTTON_2) == HIGH ||
                          digitalRead(BUTTON_3) == HIGH;

  if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD || fed4TouchAnyPadActive(TOUCH_THRESHOLD))
  {
    lastWakeSource = FedWakeSource::Touch;
  }
  else if (buttonHigh)
  {
    lastWakeSource = FedWakeSource::Button;
  }
  else
  {
    lastWakeSource = FedWakeSource::Timer;
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

  // Restore INT_OR + button GPIO wake for awake code paths
  gpio_wakeup_enable((gpio_num_t)INT_OR, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_2, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_3, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
}

FedEvent FED4::waitUntil(uint32_t updateIntervalSeconds)
{
  FedEvent event;

  const int savedSeconds = sleepSeconds;
  sleepSeconds = (updateIntervalSeconds > 0) ? (int)updateIntervalSeconds : 1;

  noPix();
  startSleep();
  wakeUp();

  // After PSV2 is back: coarse late retrieval on timer/touch/button wake
  checkLateRetrieval();

  sleepSeconds = savedSeconds;

  event.source = lastWakeSource;

  if (leftTouch)
  {
    event.source = FedWakeSource::Touch;
    event.pad = FedPad::Left;
  }
  else if (centerTouch)
  {
    event.source = FedWakeSource::Touch;
    event.pad = FedPad::Center;
  }
  else if (rightTouch)
  {
    event.source = FedWakeSource::Touch;
    event.pad = FedPad::Right;
  }
  else if (wakePad >= 1 && wakePad <= 3)
  {
    event.source = FedWakeSource::Touch;
    event.pad = static_cast<FedPad>(wakePad);
  }
  else if (lastWakeSource == FedWakeSource::Button)
  {
    if (digitalRead(BUTTON_1) == HIGH)
    {
      event.button = 1;
    }
    else if (digitalRead(BUTTON_2) == HIGH)
    {
      event.button = 2;
    }
    else if (digitalRead(BUTTON_3) == HIGH)
    {
      event.button = 3;
    }
  }

  // Library owns poke SD rows so sketches (e.g. BasicFED4) stay event-only
  if (event.source == FedWakeSource::Touch)
  {
    if (event.pad == FedPad::Left)
    {
      logData("Left");
    }
    else if (event.pad == FedPad::Center)
    {
      logData("Center");
    }
    else if (event.pad == FedPad::Right)
    {
      logData("Right");
    }
  }

  // Refresh UI while poke flags are still set (dots / counters / clock)
  update();

  return event;
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp()
{
  wakeCount++;

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD &&
      lastWakeSource != FedWakeSource::Touch)
  {
    wakePad = 0;
  }

  PSV2_ON();
  PSV3_ON();
  delay(1);
  restorePhotogateInputs();
  restoreSpiAfterPsv2On();
  // SD lost VCC with PSV2; remount so poke/log paths don't rely on hot-swap recovery
  if (sdCardAvailable)
  {
    if (!SD.begin(SD_CS, SPI, 4000000) || SD.cardType() == CARD_NONE)
    {
      Serial.println("SD remount after wake failed");
    }
  }
  i2cReinitBus();
  delay(1);

  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  enableAmp(true);

  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO && interruptPending())
  {
    lastInterruptMask = scanAndClearInterrupts();
    Serial.printf("INT_OR wake: sources = 0x%02X\n", lastInterruptMask);
  }
  else
  {
    lastInterruptMask = INT_SRC_NONE;
  }

  // Touch first (UT identifies pad immediately after touchpad wake)
  if (wakeCause == ESP_SLEEP_WAKEUP_TOUCHPAD ||
      lastWakeSource == FedWakeSource::Touch ||
      fed4TouchAnyPadActive(TOUCH_THRESHOLD))
  {
    capturePoke();
  }

  if (lastWakeSource == FedWakeSource::Button ||
      (wakeCause == ESP_SLEEP_WAKEUP_GPIO && !interruptPending()))
  {
    checkButton1();
    checkButton2();
    checkButton3();
  }

  // Sensors refreshed in update() — keep wakeUp to rails / I2C / touch / buttons
  noPix();
}
