#include "FED4.h"

// High-level sleep function that handles device sleep and wake cycle
void FED4::sleep(int seconds) {
  sleepSeconds = seconds;
  noPix();
  startSleep();
  wakeUp();
}

// For backward compatibility, keep the parameterless version
void FED4::sleep() {
  sleep(sleepSeconds);
}

// Light sleep aligned with FED4-Touch-Light-Sleep-Multiple / FED4-SleepModes:
//   - touchpad + timer only (GPIO wake disabled for the session — UT has no GPIO wake;
//     INT_OR/USB-level GPIO spam was preventing real sleep so touch never won)
//   - buttons polled between VCOM chunks
//   - software touch poll between chunks as backup if pad is held across a timer wake
//   - wall-clock deadline (millis)
void FED4::startSleep() {
  lastWakeSource = FedWakeSource::None;

  // Wait for all touch pads to be released before sleeping
  while (!fed4TouchPadsReleased(TOUCH_THRESHOLD)) {
    delay(1);
  }

  // Rare rebaseline (skip wakeCount==0 — first sleep already calibrated at begin)
  if (program != "ActivityMonitor" && wakeCount > 0 && wakeCount % 200 == 0) {
    calibrateTouchSensors();
    Serial.println("********** Touch sensors calibrated **********");
    delay(1);
  }

  // Clear poke latches and push to MIP — panel retains pixels until refresh
  resetTouchFlags();
  wakePad = 0;
  if (displayBuffer != nullptr) {
    displayIndicators();
    refresh();
  }

  if (sleepyLEDs) {
    lightsOff();
    noPix();
    PSV3_OFF();
  }

  enableAmp(false);

  if (sleepSeconds <= 0) {
    lastWakeSource = FedWakeSource::Timer;
    return;
  }

  // Match unit tests: do not arm GPIO wake during light sleep. Buttons are polled
  // between chunks; INT_OR held low would otherwise exit every sleep immediately.
  if (interruptPending()) {
    printInterruptStatus("pre-sleep");
  }
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

  fed4TouchEnableTouchpadWakeup();

  const uint32_t vcomMs = 500;
  const uint32_t deadline = millis() + (uint32_t)sleepSeconds * 1000UL;
  bool exitedForEvent = false;

  while ((int32_t)(deadline - millis()) > 0) {
    vcom = !vcom;
    digitalWrite(DISPLAY_VCOM, vcom ? HIGH : LOW);

    uint32_t remainingMs = deadline - millis();
    uint32_t chunkMs = (remainingMs < vcomMs) ? remainingMs : vcomMs;
    if (chunkMs < 1) {
      chunkMs = 1;
    }

    esp_sleep_enable_timer_wakeup((uint64_t)chunkMs * 1000ULL);
    fed4TouchEnableTouchpadWakeup();

    Serial.flush();
    esp_light_sleep_start();

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD) {
      lastWakeSource = FedWakeSource::Touch;
      exitedForEvent = true;
      break;
    }

    // Software backup: pad active after a timer wake (finger across chunk boundary)
    if (fed4TouchAnyPadActive(TOUCH_THRESHOLD)) {
      lastWakeSource = FedWakeSource::Touch;
      exitedForEvent = true;
      break;
    }

    if (digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_2) == HIGH ||
        digitalRead(BUTTON_3) == HIGH) {
      PSV2_ON();
      i2cReinitBus();
      checkButton1();
      checkButton2();
      checkButton3();
      lastWakeSource = FedWakeSource::Button;
      exitedForEvent = true;
      break;
    }

    updateStatusLedFromMotion();
  }

  if (!exitedForEvent) {
    lastWakeSource = FedWakeSource::Timer;
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

  // Restore GPIO wake for buttons / INT_OR outside the sleep session
  esp_sleep_enable_gpio_wakeup();
}

FedEvent FED4::waitUntil(uint32_t housekeepingSeconds) {
  FedEvent event;

  const int savedSeconds = sleepSeconds;
  sleepSeconds = (housekeepingSeconds > 0) ? (int)housekeepingSeconds : 1;

  noPix();
  startSleep();
  wakeUp();

  sleepSeconds = savedSeconds;

  event.source = lastWakeSource;

  if (leftTouch) {
    event.source = FedWakeSource::Touch;
    event.pad = 1;
  } else if (centerTouch) {
    event.source = FedWakeSource::Touch;
    event.pad = 2;
  } else if (rightTouch) {
    event.source = FedWakeSource::Touch;
    event.pad = 3;
  } else if (wakePad >= 1 && wakePad <= 3) {
    event.source = FedWakeSource::Touch;
    event.pad = wakePad;
  } else if (lastWakeSource == FedWakeSource::Button) {
    if (digitalRead(BUTTON_1) == HIGH) {
      event.button = 1;
    } else if (digitalRead(BUTTON_2) == HIGH) {
      event.button = 2;
    } else if (digitalRead(BUTTON_3) == HIGH) {
      event.button = 3;
    }
  }

  return event;
}

// Wakes up device by re-enabling components and initializing I2C/I2S
void FED4::wakeUp() {
  wakeCount++;

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD &&
      lastWakeSource != FedWakeSource::Touch) {
    wakePad = 0;
  }

  PSV2_ON();
  i2cReinitBus();
  delay(1);

  mcp.pinMode(EXP_HAPTIC, OUTPUT);
  mcp.digitalWrite(EXP_HAPTIC, LOW);

  PSV3_ON();
  enableAmp(true);

  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO && interruptPending()) {
    lastInterruptMask = scanAndClearInterrupts();
    Serial.printf("INT_OR wake: sources = 0x%02X\n", lastInterruptMask);
  } else {
    lastInterruptMask = INT_SRC_NONE;
  }

  // Touch first (UT identifies pad immediately after touchpad wake)
  if (wakeCause == ESP_SLEEP_WAKEUP_TOUCHPAD ||
      lastWakeSource == FedWakeSource::Touch ||
      fed4TouchAnyPadActive(TOUCH_THRESHOLD)) {
    interpretTouch();
  }

  // App-level wake: buttons + sensors (not every VCOM micro-wake)
  if (wakeCause != ESP_SLEEP_WAKEUP_TOUCHPAD &&
      lastWakeSource != FedWakeSource::Touch) {
    checkButton1();
    checkButton2();
    checkButton3();

    if (program == "ActivityMonitor") {
      pollSensors(1);
    } else {
      pollSensors(10);
    }
  }

  if (useMotionSensor) {
    updateStatusLedFromMotion();
  } else {
    redPix(1);
  }
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
