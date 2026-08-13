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

// SleepModes / Demo-Hardware VCOM keepalive between light-sleep chunks (ms).
static const uint32_t kVcomChunkMs = 500;

static void toggleVcomGpio(bool &vcomLevel)
{
  vcomLevel = !vcomLevel;
  pinMode(DISPLAY_VCOM, OUTPUT);
  digitalWrite(DISPLAY_VCOM, vcomLevel ? HIGH : LOW);
}

// Minimal SD/MIP CS park before PSV2 off — no SPI.end() / gpio_hold (Test B).
static void parkSpiChipSelectsForPsv2Off()
{
  SD.end();
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, LOW);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
}

static void quiescePsv2PeripheralGpios()
{
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

// DIAG Test B: PSV2 OFF + GPIO VCOM every 500 ms chunk (no LEDC, no SPI.end/hold).
// If flicker returns → cutting 3.3V2 / unpowered SD on shared SCK/MOSI.
// Poke logData gated by FED4_DIAG_SKIP_SD_LOG (0 = enabled).
void FED4::startSleep()
{
  lastWakeSource = FedWakeSource::None;

  releaseVcomLedcToGpio(); // ensure GPIO VCOM (no LEDC during Test B)

  while (!fed4TouchPadsReleased(TOUCH_THRESHOLD))
  {
    delay(1);
  }

  if (wakeCount > 0 && wakeCount % 200 == 0)
  {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    delay(1);
  }

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

  Serial.println("DIAG Test B: PSV2 off + GPIO VCOM 500 ms chunks (no SPI.end/hold)");
  parkSpiChipSelectsForPsv2Off();
  quiescePsv2PeripheralGpios();
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

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  gpio_wakeup_disable((gpio_num_t)INT_OR);
  gpio_wakeup_disable((gpio_num_t)PHOTOGATE_1);
  gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_2, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_3, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  fed4TouchEnableTouchpadWakeup();

  uint32_t remainingMs = (uint32_t)sleepSeconds * 1000UL;
  while (remainingMs > 0)
  {
    toggleVcomGpio(vcom);

    const uint32_t chunkMs = remainingMs < kVcomChunkMs ? remainingMs : kVcomChunkMs;
    esp_sleep_enable_timer_wakeup((uint64_t)chunkMs * 1000ULL);

    Serial.flush();
    esp_light_sleep_start();

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const bool buttonHigh = digitalRead(BUTTON_1) == HIGH ||
                            digitalRead(BUTTON_2) == HIGH ||
                            digitalRead(BUTTON_3) == HIGH;
    const bool touchWake = (cause == ESP_SLEEP_WAKEUP_TOUCHPAD) ||
                           fed4TouchAnyPadActive(TOUCH_THRESHOLD);

    if (touchWake)
    {
      lastWakeSource = FedWakeSource::Touch;
      break;
    }
    if (buttonHigh || cause == ESP_SLEEP_WAKEUP_GPIO)
    {
      lastWakeSource = FedWakeSource::Button;
      break;
    }

    if (remainingMs > chunkMs)
    {
      remainingMs -= chunkMs;
    }
    else
    {
      remainingMs = 0;
      lastWakeSource = FedWakeSource::Timer;
    }
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

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

#if !FED4_DIAG_SKIP_SD_LOG
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
#else
  if (event.source == FedWakeSource::Touch)
  {
    Serial.println("DIAG: skip poke logData (FED4_DIAG_SKIP_SD_LOG)");
  }
#endif

  update();

  return event;
}

void FED4::wakeUp()
{
  wakeCount++;

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD &&
      lastWakeSource != FedWakeSource::Touch)
  {
    wakePad = 0;
  }

  // Test B: rail was off — restore PSV2, photogates, remount SD
  PSV2_ON();
  PSV3_ON();
  delay(40);

  pinMode(PHOTOGATE_1, INPUT_PULLUP);
  pinMode(PHOTOGATE_2, INPUT_PULLUP);
  pinMode(PHOTOGATE_3, INPUT_PULLUP);
  pinMode(PHOTOGATE_4, INPUT_PULLUP);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  if (sdCardAvailable)
  {
    if (!SD.begin(SD_CS, SPI, 4000000) || SD.cardType() == CARD_NONE)
    {
      Serial.println("SD remount after wake failed");
    }
  }
  reclaimSpiForDisplay();

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

  noPix();
}
