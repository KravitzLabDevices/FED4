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

// Light sleep with LEDC VCOM keepalive (FED4-VCOM-LEDC-Light-Sleep):
//   - one timer wake for the full sleepSeconds budget (no 500 ms VCOM chunks)
//   - touchpad wake + button-only GPIO wake (INT_OR disabled for the session)
//   - software touch poll after wake as backup
void FED4::startSleep()
{
  lastWakeSource = FedWakeSource::None;

  // Wait for all touch pads to be released before sleeping
  while (!fed4TouchPadsReleased(TOUCH_THRESHOLD))
  {
    delay(1);
  }

  // Rare rebaseline (skip wakeCount==0 — first sleep already calibrated at begin)
  if (program != "ActivityMonitor" && wakeCount > 0 && wakeCount % 200 == 0)
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

  if (sleepyLEDs)
  {
    lightsOff();
    noPix();
    PSV3_OFF();
  }

  enableAmp(false);

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
  gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_2, GPIO_INTR_HIGH_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BUTTON_3, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  fed4TouchEnableTouchpadWakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

  Serial.flush();
  esp_light_sleep_start();

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD || fed4TouchAnyPadActive(TOUCH_THRESHOLD))
  {
    lastWakeSource = FedWakeSource::Touch;
  }
  else if (cause == ESP_SLEEP_WAKEUP_GPIO ||
           digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_2) == HIGH ||
           digitalRead(BUTTON_3) == HIGH)
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
  i2cReinitBus();
  delay(1);

  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  PSV3_ON();
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
    interpretTouch();
  }

  if (lastWakeSource == FedWakeSource::Button ||
      (wakeCause == ESP_SLEEP_WAKEUP_GPIO && !interruptPending()))
  {
    checkButton1();
    checkButton2();
    checkButton3();
  }

  // Sensor poll on timer/housekeeping wakes (not every touch)
  if (lastWakeSource != FedWakeSource::Touch)
  {
    if (program == "ActivityMonitor")
    {
      pollSensors(1);
    }
    else
    {
      pollSensors(10);
    }
  }

  // STATUS_LED left alone here — digital only via redPix/noPix; not tied to PIR
  noPix();
}

bool FED4::initializePower()
{
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  PSV2_ON();
  PSV3_ON();
  return true;
}

void FED4::PSV2_ON()
{
  mcp.digitalWrite(EXP_PSV2_EN, LOW); // active LOW enable
}

void FED4::PSV2_OFF()
{
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
}

void FED4::PSV3_ON()
{
  mcp.digitalWrite(EXP_PSV3_EN, LOW); // active LOW enable
}

void FED4::PSV3_OFF()
{
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);
}
