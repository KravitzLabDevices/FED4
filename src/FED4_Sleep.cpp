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

// PSV2 ON + one long light sleep + LEDC VCOM KEEP_ALIVE (no rail-off SPI teardown).
// Poke logData gated by FED4_DIAG_SKIP_SD_LOG (0 = enabled).
void FED4::startSleep()
{
  lastWakeSource = FedWakeSource::None;

  // Wait for all touch pads to be released before sleeping.
  // If idle drifted (false "touch"), re-characterize once after 2s so we
  // don't hang forever — sampling the elevated baseline clears the stuck rise.
  Serial.println("startSleep: waiting for pads released...");
  Serial.flush();
  {
    const uint32_t waitStartMs = millis();
    uint32_t lastDiagMs = waitStartMs;
    bool didRescueChar = false;
    while (!fed4TouchPadsReleased(TOUCH_THRESHOLD))
    {
      const uint32_t nowMs = millis();
      if (!didRescueChar && (nowMs - waitStartMs) >= 2000)
      {
        didRescueChar = true;
        Serial.println("startSleep: pads stuck — re-characterizing baselines...");
        Serial.flush();
        (void)fed4TouchCharacterizePads();
        continue;
      }
      if ((nowMs - lastDiagMs) >= 1000)
      {
        lastDiagMs = nowMs;
        const uint32_t rawL = fed4TouchRead(TOUCH_PAD_LEFT);
        const uint32_t rawC = fed4TouchRead(TOUCH_PAD_CENTER);
        const uint32_t rawR = fed4TouchRead(TOUCH_PAD_RIGHT);
        Serial.printf(
            "startSleep: pads NOT released after %lu ms | "
            "L raw=%lu idle=%lu rise=%.3f thr=%.3f | "
            "C raw=%lu idle=%lu rise=%.3f thr=%.3f | "
            "R raw=%lu idle=%lu rise=%.3f thr=%.3f\n",
            (unsigned long)(nowMs - waitStartMs),
            (unsigned long)rawL, (unsigned long)fed4TouchIdleL,
            (double)fed4TouchRiseFraction(rawL, fed4TouchIdleL),
            (double)fed4TouchRiseThreshL,
            (unsigned long)rawC, (unsigned long)fed4TouchIdleC,
            (double)fed4TouchRiseFraction(rawC, fed4TouchIdleC),
            (double)fed4TouchRiseThreshC,
            (unsigned long)rawR, (unsigned long)fed4TouchIdleR,
            (double)fed4TouchRiseFraction(rawR, fed4TouchIdleR),
            (double)fed4TouchRiseThreshR);
        Serial.flush();
      }
      delay(1);
    }
    Serial.printf("startSleep: pads released after %lu ms\n",
                  (unsigned long)(millis() - waitStartMs));
    Serial.flush();
  }

  // Clear poke latches and push to MIP — panel retains pixels until refresh
  resetTouchFlags();
  wakePad = 0;
  if (displayBuffer != nullptr)
  {
    displayIndicators();
    refresh(); // GPIO VCOM invert for this frame
  }

  noPix();
  enableAmp(false); // EXP_AMP_SD LOW; PSV2 stays on

  if (sleepyLEDs)
  {
    lightsOff();
    PSV3_OFF();
  }

  // 3.3V2 stays powered — photogates / SD keep VCC (no SPI.end / pin-hold)
  PSV2_ON();

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

  fed4TouchClearWakePadLatch();
  fed4TouchEnableTouchpadWakeup();

  // Continuous VCOM via LEDC through one full-duration light sleep
  startVcomLedc();
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

  Serial.printf("startSleep: entering light sleep (%d s timer + touch/button wake)\n",
                sleepSeconds);
  Serial.flush();
  esp_light_sleep_start();

  fed4PokeTimingReset();
  fed4PokeTimingMark(FED4_POKE_T_WAKE);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool buttonHigh = digitalRead(BUTTON_1) == HIGH ||
                          digitalRead(BUTTON_2) == HIGH ||
                          digitalRead(BUTTON_3) == HIGH;

  Serial.printf("startSleep: woke cause=%d buttonHigh=%d\n", (int)cause,
                (int)buttonHigh);
  Serial.flush();

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

  fed4PokeTimingMark(FED4_POKE_T_CLASSIFIED);

  // LED only when a poke pad was resolved (not timer / not touch-wake without ID)
  if (event.source == FedWakeSource::Touch && event.pad != FedPad::None)
  {
    redPix(1);
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
  fed4PokeTimingMark(FED4_POKE_T_LOG_DONE);

  // Poke: skip sensors + full redraw (counters/indicators only). Timer/button: full.
  const bool pokeFast = (event.source == FedWakeSource::Touch &&
                         event.pad != FedPad::None);
  update(pokeFast ? FedUpdateMode::Poke : FedUpdateMode::Full);
  fed4PokeTimingMark(FED4_POKE_T_UPDATE_DONE);

  if (event.source == FedWakeSource::Touch)
  {
    const char *padName = "none";
    if (event.pad == FedPad::Left)
      padName = "Left";
    else if (event.pad == FedPad::Center)
      padName = "Center";
    else if (event.pad == FedPad::Right)
      padName = "Right";
    fed4PokeTimingPrint(padName);
  }

  return event;
}

void FED4::wakeUp()
{
  wakeCount++;
  fed4PokeTimingMark(FED4_POKE_T_WAKEUP);

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD &&
      lastWakeSource != FedWakeSource::Touch)
  {
    wakePad = 0;
  }

  // Hand VCOM back to GPIO before any refresh()
  releaseVcomLedcToGpio();

  // Rails: PSV2 was left on; re-assert enables. No SPI.end / remount for rail cycle.
  PSV2_ON();
  PSV3_ON();
  delay(1);

  pinMode(PHOTOGATE_1, INPUT_PULLUP);
  pinMode(PHOTOGATE_2, INPUT_PULLUP);
  pinMode(PHOTOGATE_3, INPUT_PULLUP);
  pinMode(PHOTOGATE_4, INPUT_PULLUP);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);
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
    fed4PokeTimingMark(FED4_POKE_T_PRE_CAPTURE);
    capturePoke();
    fed4PokeTimingMark(FED4_POKE_T_CAPTURE_DONE);
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
