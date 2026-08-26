/*
 * FED4 Touch Calibrate (NVS) — IN PROGRESS / WIP
 *
 * Status: not production-ready. Wizard + NVS API exist; begin() does not
 * auto-load cal yet. Expect further UX and validation work.
 *
 * Guided wizard: button-confirmed CLEAR baselines + auto-detected prompted
 * pad touches (×2 L/C/R cycles). Saves idle + poke delta → wakeAbs to NVS
 * (Preferences namespace "fed4") so calibration follows the device.
 *
 * Cueing: MIP display, strip LEDs, speaker.
 * BUTTON_1 = confirm hands clear / start recalibrate
 * BUTTON_3 = skip wizard and monitor (if cal already present)
 *
 * Does not auto-load in begin() yet — production still uses live char unless
 * this sketch Apply()s, or a later begin() hook is added.
 */

#include <FED4.h>
#include <math.h>

FED4 fed4;

static const int CAL_REPS = 2;
static const uint32_t BTN_DEBOUNCE_MS = 80;
static const uint32_t POST_BTN_QUIET_MS = 1500;
static const uint32_t TOUCH_SUSTAIN_MS = 300;
static const uint32_t TOUCH_RELEASE_SETTLE_MS = 300;
static const float ONSET_SIGMA = 6.0f;
static const float QUIET_SIGMA = 4.0f;
static const float DELTA_AGREE = 0.20f; // reps must agree within 20%

enum PadId : int { PAD_L = 0, PAD_C = 1, PAD_R = 2 };

struct PadSample {
  uint32_t mean;
  float stddev;
};

struct RepData {
  PadSample clearIdle[3]; // after each clear, last clear before touch used
  uint32_t touchDelta[3];
};

static uint8_t padPin(PadId p) {
  if (p == PAD_L) return TOUCH_PAD_LEFT;
  if (p == PAD_C) return TOUCH_PAD_CENTER;
  return TOUCH_PAD_RIGHT;
}

static const char *padName(PadId p) {
  if (p == PAD_L) return "LEFT";
  if (p == PAD_C) return "CENTER";
  return "RIGHT";
}

static void showPrompt(const char *line1, const char *line2 = nullptr,
                       const char *line3 = nullptr) {
  fed4.clearDisplay();
  fed4.setFont(nullptr);
  fed4.setTextSize(1);
  fed4.setTextColor(DISPLAY_BLACK);
  fed4.setCursor(6, 40);
  fed4.print(line1);
  if (line2) {
    fed4.setCursor(6, 70);
    fed4.print(line2);
  }
  if (line3) {
    fed4.setCursor(6, 100);
    fed4.print(line3);
  }
  fed4.refresh();
}

static void cueClear() {
  fed4.lightsOff();
  fed4.colorWipe("white", 5);
  fed4.playTone(880, 80, 0.2f);
}

static void cueTouch(PadId p) {
  fed4.lightsOff();
  if (p == PAD_L) fed4.leftLight("green", 180);
  else if (p == PAD_C) fed4.centerLight("green", 180);
  else fed4.rightLight("green", 180);
  fed4.playTone(660, 80, 0.2f);
}

static void cueOk() { fed4.playTone(1200, 100, 0.25f); }
static void cueErr() { fed4.playTone(220, 250, 0.3f); }
static void cueDone() {
  fed4.playTone(880, 80, 0.25f);
  delay(40);
  fed4.playTone(1175, 120, 0.25f);
}

static bool button1PressedEdge(bool &wasHigh) {
  const bool high = digitalRead(BUTTON_1) == HIGH;
  bool edge = false;
  if (high && !wasHigh) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(BUTTON_1) == HIGH)
      edge = true;
  }
  wasHigh = high;
  return edge;
}

static bool button3PressedEdge(bool &wasHigh) {
  const bool high = digitalRead(BUTTON_3) == HIGH;
  bool edge = false;
  if (high && !wasHigh) {
    delay(BTN_DEBOUNCE_MS);
    if (digitalRead(BUTTON_3) == HIGH)
      edge = true;
  }
  wasHigh = high;
  return edge;
}

static void waitButton1Released() {
  while (digitalRead(BUTTON_1) == HIGH)
    delay(5);
  delay(BTN_DEBOUNCE_MS);
}

static PadSample samplePad(uint8_t pin) {
  uint32_t samples[TOUCH_CHAR_SAMPLES];
  uint64_t sum = 0;
  for (int i = 0; i < TOUCH_CHAR_SAMPLES; i++) {
    samples[i] = fed4TouchRead(pin);
    sum += samples[i];
    delay(TOUCH_CHAR_INTERVAL_MS);
  }
  PadSample out;
  const double mean = (double)sum / (double)TOUCH_CHAR_SAMPLES;
  double var = 0.0;
  for (int i = 0; i < TOUCH_CHAR_SAMPLES; i++) {
    const double d = (double)samples[i] - mean;
    var += d * d;
  }
  out.mean = (uint32_t)(mean + 0.5);
  out.stddev = (float)sqrt(var / (double)TOUCH_CHAR_SAMPLES);
  return out;
}

static void readAll(uint32_t *out) {
  out[0] = fed4TouchRead(TOUCH_PAD_LEFT);
  out[1] = fed4TouchRead(TOUCH_PAD_CENTER);
  out[2] = fed4TouchRead(TOUCH_PAD_RIGHT);
}

/** Quiet gate: all pads near provisional idle for quietMs. */
static bool waitQuiet(const uint32_t idle[3], const float stddev[3],
                      uint32_t quietMs, uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  uint32_t quietStart = 0;
  while ((millis() - t0) < timeoutMs) {
    uint32_t raw[3];
    readAll(raw);
    bool ok = true;
    for (int i = 0; i < 3; i++) {
      const float lim = QUIET_SIGMA * (stddev[i] > 1.0f ? stddev[i] : 50.0f) + 200.0f;
      const float d = fabsf((float)raw[i] - (float)idle[i]);
      if (d > lim) {
        ok = false;
        break;
      }
    }
    if (ok) {
      if (!quietStart)
        quietStart = millis();
      if ((millis() - quietStart) >= quietMs)
        return true;
    } else {
      quietStart = 0;
    }
    delay(10);
  }
  return false;
}

static bool sampleClearBaseline(PadSample outIdle[3]) {
  showPrompt("CLEAR — hands away", "Press BUTTON 1", "then wait for sample");
  cueClear();

  bool wasHigh = digitalRead(BUTTON_1) == HIGH;
  while (!button1PressedEdge(wasHigh))
    delay(5);
  waitButton1Released();

  showPrompt("Hold still...", "Sampling quiet", nullptr);
  // Provisional idle for quiet gate
  PadSample prov[3];
  for (int i = 0; i < 3; i++)
    prov[i] = samplePad(padPin((PadId)i));

  uint32_t idle[3] = {prov[0].mean, prov[1].mean, prov[2].mean};
  float stdv[3] = {prov[0].stddev, prov[1].stddev, prov[2].stddev};

  if (!waitQuiet(idle, stdv, POST_BTN_QUIET_MS, 20000)) {
    showPrompt("CLEAR failed", "Still noisy", "Retry step");
    cueErr();
    delay(800);
    return false;
  }

  for (int i = 0; i < 3; i++)
    outIdle[i] = samplePad(padPin((PadId)i));

  Serial.printf("Clear idle L/C/R %lu/%lu/%lu\n",
                (unsigned long)outIdle[0].mean, (unsigned long)outIdle[1].mean,
                (unsigned long)outIdle[2].mean);
  cueOk();
  return true;
}

static bool sampleTouchDelta(PadId target, const PadSample idle[3],
                             uint32_t *outDelta) {
  char line[32];
  snprintf(line, sizeof(line), "TOUCH %s", padName(target));
  showPrompt(line, "Hold ~0.3s then release", "Prompted pad only");
  cueTouch(target);

  uint32_t idleU[3] = {idle[0].mean, idle[1].mean, idle[2].mean};
  float stdv[3] = {idle[0].stddev, idle[1].stddev, idle[2].stddev};

  if (!waitQuiet(idleU, stdv, 400, 15000)) {
    cueErr();
    return false;
  }

  // Wait onset on prompted pad
  const uint32_t t0 = millis();
  bool got = false;
  uint64_t peakSum = 0;
  int peakN = 0;
  uint32_t sustainStart = 0;

  while ((millis() - t0) < 30000) {
    uint32_t raw[3];
    readAll(raw);
    float d[3];
    for (int i = 0; i < 3; i++) {
      d[i] = (raw[i] > idleU[i]) ? (float)(raw[i] - idleU[i]) : 0.0f;
    }
    const float thr =
        ONSET_SIGMA * (stdv[target] > 1.0f ? stdv[target] : 50.0f) + 400.0f;

    // Cross-talk: other pad nearly as strong
    int strongest = 0;
    for (int i = 1; i < 3; i++) {
      if (d[i] > d[strongest])
        strongest = i;
    }

    if (!got) {
      if (d[target] >= thr && strongest == (int)target) {
        got = true;
        sustainStart = millis();
        peakSum = raw[target];
        peakN = 1;
        fed4.click();
      }
    } else {
      if (strongest != (int)target && d[strongest] > d[target] * 0.85f) {
        showPrompt("Cross-talk", "Retry pad", padName(target));
        cueErr();
        delay(600);
        return false;
      }
      peakSum += raw[target];
      peakN++;
      if ((millis() - sustainStart) >= TOUCH_SUSTAIN_MS) {
        // Wait release
        const uint32_t touchMean = (uint32_t)(peakSum / (uint64_t)peakN);
        if (touchMean <= idleU[target]) {
          cueErr();
          return false;
        }
        *outDelta = touchMean - idleU[target];

        const uint32_t r0 = millis();
        while ((millis() - r0) < 10000) {
          uint32_t rr[3];
          readAll(rr);
          float lim = QUIET_SIGMA * (stdv[target] > 1.0f ? stdv[target] : 50.0f) + 200.0f;
          if (fabsf((float)rr[target] - (float)idleU[target]) < lim) {
            delay(TOUCH_RELEASE_SETTLE_MS);
            Serial.printf("Touch %s delta=%lu\n", padName(target),
                          (unsigned long)*outDelta);
            cueOk();
            return true;
          }
          delay(10);
        }
        cueErr();
        return false;
      }
    }
    delay(10);
  }
  showPrompt("Timeout", "No touch detected", padName(target));
  cueErr();
  return false;
}

static uint32_t median2(uint32_t a, uint32_t b) {
  return (a + b) / 2;
}

static float median2f(float a, float b) { return 0.5f * (a + b); }

static bool deltasAgree(uint32_t a, uint32_t b) {
  if (!a || !b)
    return false;
  const uint32_t lo = (a < b) ? a : b;
  const uint32_t hi = (a > b) ? a : b;
  return (float)(hi - lo) <= DELTA_AGREE * (float)hi;
}

static bool runWizard(Fed4TouchCal *outCal) {
  RepData reps[CAL_REPS] = {};

  for (int rep = 0; rep < CAL_REPS; rep++) {
    char repLine[24];
    snprintf(repLine, sizeof(repLine), "Rep %d / %d", rep + 1, CAL_REPS);
    Serial.println(repLine);

    PadSample clearBefore[3];
    PadId order[3] = {PAD_L, PAD_C, PAD_R};

    for (int step = 0; step < 3; step++) {
      PadSample clearSamp[3];
      while (!sampleClearBaseline(clearSamp))
        delay(200);
      for (int i = 0; i < 3; i++)
        clearBefore[i] = clearSamp[i];

      uint32_t delta = 0;
      while (!sampleTouchDelta(order[step], clearBefore, &delta))
        delay(200);
      reps[rep].touchDelta[order[step]] = delta;
      reps[rep].clearIdle[order[step]] = clearBefore[order[step]];
    }

    // Final clear after last touch of rep
    PadSample finalClear[3];
    while (!sampleClearBaseline(finalClear))
      delay(200);
    // Prefer last clear for idle merge contribution
    for (int i = 0; i < 3; i++)
      reps[rep].clearIdle[i] = finalClear[i];
  }

  // Merge
  Fed4TouchCal cal = {};
  cal.ver = FED4_TOUCH_CAL_VER;
  fed4TouchCalSetMap(&cal);
  cal.unixTime = (uint32_t)(millis() / 1000UL);

  Fed4TouchCalPad *pads[3] = {&cal.L, &cal.C, &cal.R};
  for (int i = 0; i < 3; i++) {
    if (!deltasAgree(reps[0].touchDelta[i], reps[1].touchDelta[i])) {
      showPrompt("Delta mismatch", padName((PadId)i), "Run again");
      Serial.printf("Disagree %s d0=%lu d1=%lu\n", padName((PadId)i),
                    (unsigned long)reps[0].touchDelta[i],
                    (unsigned long)reps[1].touchDelta[i]);
      cueErr();
      return false;
    }
    pads[i]->idleMean =
        median2(reps[0].clearIdle[i].mean, reps[1].clearIdle[i].mean);
    pads[i]->idleStd =
        median2f(reps[0].clearIdle[i].stddev, reps[1].clearIdle[i].stddev);
    pads[i]->touchDelta = median2(reps[0].touchDelta[i], reps[1].touchDelta[i]);
    fed4TouchCalDerivePad(pads[i]);
  }

  if (!fed4TouchCalValid(&cal)) {
    showPrompt("Cal invalid", "Check samples", nullptr);
    cueErr();
    return false;
  }

  *outCal = cal;
  return true;
}

static void showCalSummary(const Fed4TouchCal &cal) {
  fed4.clearDisplay();
  fed4.setFont(nullptr);
  fed4.setTextSize(1);
  fed4.setTextColor(DISPLAY_BLACK);
  fed4.setCursor(4, 28);
  fed4.print("NVS Touch Cal OK");
  char buf[40];
  snprintf(buf, sizeof(buf), "L d=%lu w=%lu", (unsigned long)cal.L.touchDelta,
           (unsigned long)cal.L.wakeAbs);
  fed4.setCursor(4, 56);
  fed4.print(buf);
  snprintf(buf, sizeof(buf), "C d=%lu w=%lu", (unsigned long)cal.C.touchDelta,
           (unsigned long)cal.C.wakeAbs);
  fed4.setCursor(4, 80);
  fed4.print(buf);
  snprintf(buf, sizeof(buf), "R d=%lu w=%lu", (unsigned long)cal.R.touchDelta,
           (unsigned long)cal.R.wakeAbs);
  fed4.setCursor(4, 104);
  fed4.print(buf);
  fed4.setCursor(4, 140);
  fed4.print("BTN1=recal  BTN3=mon");
  fed4.refresh();
  fed4TouchCalPrint(&cal);
}

static void liveMonitor() {
  showPrompt("Live monitor", "rise L/C/R", "reset to recal");
  fed4.lightsOff();
  while (true) {
    if (digitalRead(BUTTON_1) == HIGH) {
      delay(BTN_DEBOUNCE_MS);
      if (digitalRead(BUTTON_1) == HIGH)
        return;
    }
    const uint32_t l = fed4TouchRead(TOUCH_PAD_LEFT);
    const uint32_t c = fed4TouchRead(TOUCH_PAD_CENTER);
    const uint32_t r = fed4TouchRead(TOUCH_PAD_RIGHT);
    Serial.printf("L:%lu rise:%.3f | C:%lu rise:%.3f | R:%lu rise:%.3f\n",
                  (unsigned long)l,
                  fed4TouchRiseFraction(l, fed4TouchIdleL),
                  (unsigned long)c,
                  fed4TouchRiseFraction(c, fed4TouchIdleC),
                  (unsigned long)r,
                  fed4TouchRiseFraction(r, fed4TouchIdleR));
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!fed4.begin("TouchCal")) {
    Serial.println("begin() failed");
    while (true)
      delay(1000);
  }

  Fed4TouchCal existing = {};
  const bool haveCal = fed4.touchCalLoad(&existing);

  if (haveCal) {
    fed4.touchCalApply(existing);
    showCalSummary(existing);
    cueDone();

    bool w1 = digitalRead(BUTTON_1) == HIGH;
    bool w3 = digitalRead(BUTTON_3) == HIGH;
    while (true) {
      if (button1PressedEdge(w1)) {
        waitButton1Released();
        break; // recalibrate
      }
      if (button3PressedEdge(w3)) {
        liveMonitor();
        showCalSummary(existing);
        w1 = digitalRead(BUTTON_1) == HIGH;
        w3 = digitalRead(BUTTON_3) == HIGH;
      }
      delay(5);
    }
  }

  // Run wizard (no cal, or user chose recal)
  while (true) {
    showPrompt("Touch calibrate", "2x L/C/R cycles", "Follow prompts");
    delay(600);

    Fed4TouchCal cal = {};
    if (!runWizard(&cal)) {
      showPrompt("Wizard failed", "BTN1 retry", nullptr);
      cueErr();
      bool w1 = false;
      while (!button1PressedEdge(w1))
        delay(5);
      waitButton1Released();
      continue;
    }

    if (!fed4.touchCalSave(cal) || !fed4.touchCalApply(cal)) {
      showPrompt("Save/Apply failed", "BTN1 retry", nullptr);
      cueErr();
      bool w1 = false;
      while (!button1PressedEdge(w1))
        delay(5);
      waitButton1Released();
      continue;
    }

    showCalSummary(cal);
    cueDone();
    break;
  }
}

void loop() {
  bool w1 = digitalRead(BUTTON_1) == HIGH;
  bool w3 = digitalRead(BUTTON_3) == HIGH;
  Fed4TouchCal cal = {};
  const bool have = fed4.touchCalLoad(&cal);

  if (button1PressedEdge(w1)) {
    waitButton1Released();
    // Re-enter wizard via soft reset path
    Fed4TouchCal neu = {};
    if (runWizard(&neu) && fed4.touchCalSave(neu) && fed4.touchCalApply(neu)) {
      showCalSummary(neu);
      cueDone();
    }
  } else if (have && button3PressedEdge(w3)) {
    liveMonitor();
    showCalSummary(cal);
  }
  delay(20);
}
