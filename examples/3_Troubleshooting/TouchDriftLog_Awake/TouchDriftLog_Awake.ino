/*
  FED4 Touch Drift Log — AWAKE arm (bench physics)

  Never sleeps. Shows the real-time pad physics that the light-sleep interrupt
  path hides, writing the SAME _T.CSV schema as TouchDriftLog_Sleep so the two
  arms concatenate in the analysis script and differ only in the Mode column.

    1/min  Heartbeat row to SD (idle / benchmark / thresholds)
    10 Hz  Serial line for bench work
    poke   software-detected on absolute (smooth − idle), logged on release with
           PeakSmooth (peak amplitude) and PokeDuration (hold ms); L/C/R counters
           and indicator dots updated on the display (FedUpdateMode::Poke)

  If this arm stays healthy while the sleep arm dies, the fault is in the light
  sleep / benchmark / latch path — not in the pad.

  REQUIRES FED4_ENABLE_TOUCH_LOG = 1 in src/FED4.h — it is a LIBRARY flag, so a
  #define here does NOT reach the library under the Arduino IDE. Production
  default is 0; rebuild the library with =1 for a diagnostic campaign.

  Display: Task field = "AwakeDrift"; footer = firmware v1.7.1
  (see docs/firmware/v1.7.1.md). Firmware number bumps when src/ changes.

  Analyse with extras/analysis/fed4_touch_analysis.py.
*/

#include <FED4.h>

FED4 fed4;

static const uint32_t SD_HEARTBEAT_MS = 60000; // Heartbeat row to SD (1/min)
static const uint32_t SERIAL_PRINT_MS = 100;  // 10 Hz bench print
static const uint32_t SENSOR_POLL_MS = 60000; // keep ENV/battery covariates fresh

// Software poke state machine (one pad at a time — strongest absolute delta)
static int activePad = 0;             // 0 = none, 1/2/3 = L/C/R
static uint32_t activePeakSmooth = 0; // peak smooth count during the hold
static unsigned long activeStartMs = 0;

static unsigned long lastHeartbeatMs = 0;
static unsigned long lastPrintMs = 0;
static unsigned long lastSensorMs = 0;

/** Strongest pad whose absolute (smooth − idle) clears its wake threshold. */
static int detectPad(const uint32_t smooth[3])
{
  const uint32_t idles[3] = {fed4TouchIdleL, fed4TouchIdleC, fed4TouchIdleR};
  const uint32_t floors[3] = {
      fed4TouchWakeAbsL ? fed4TouchWakeAbsL : (uint32_t)TOUCH_CHAR_ABS_MIN,
      fed4TouchWakeAbsC ? fed4TouchWakeAbsC : (uint32_t)TOUCH_CHAR_ABS_MIN,
      fed4TouchWakeAbsR ? fed4TouchWakeAbsR : (uint32_t)TOUCH_CHAR_ABS_MIN};

  int bestPad = 0;
  int32_t bestDelta = -1;
  for (int i = 0; i < 3; i++)
  {
    if (!idles[i])
      continue;
    const int32_t d = (int32_t)smooth[i] - (int32_t)idles[i];
    if (d >= (int32_t)floors[i] && d > bestDelta)
    {
      bestDelta = d;
      bestPad = i + 1;
    }
  }
  return bestPad;
}

void setup()
{
  // Set before begin() so the BootChar row is labelled correctly
  fed4.touchLogMode = "Awake";

  fed4.begin("AwakeDrift");

#if !FED4_ENABLE_TOUCH_LOG
  Serial.println("WARNING: FED4_ENABLE_TOUCH_LOG is 0 — no touch log will be written.");
  Serial.println("         Set it to 1 in src/FED4.h and rebuild the library.");
#else
  if (!fed4.isTouchLogAvailable())
  {
    Serial.println("WARNING: touch log file was not created — check the SD card.");
  }
#endif

  fed4TouchPrintCharacterization();
  fed4TouchPrintDriverConfig();
  Serial.println("Awake arm: 1/min Heartbeat rows to SD, 10 Hz Serial, software poke rows.");

  lastSensorMs = millis();
}

void loop()
{
  const unsigned long nowMs = millis();

  const uint32_t smooth[3] = {fed4TouchRead(TOUCH_PAD_LEFT),
                              fed4TouchRead(TOUCH_PAD_CENTER),
                              fed4TouchRead(TOUCH_PAD_RIGHT)};

  // ── Software poke detection ────────────────────────────────────────────────
  const int pad = detectPad(smooth);

  if (pad && !activePad)
  {
    activePad = pad;
    activeStartMs = nowMs;
    activePeakSmooth = smooth[pad - 1];
  }
  else if (activePad)
  {
    if (smooth[activePad - 1] > activePeakSmooth)
      activePeakSmooth = smooth[activePad - 1];

    if (!pad)
    {
      // Release — publish the poke into the shared touch-log snapshot.
      // No latch here (no interrupt path awake), so LatchPad is 0.
      fed4TouchSetLastPoke(0, activePad, activePeakSmooth);
      FED4::wakePad = (uint8_t)activePad;
      fed4.pokeDuration = (float)(nowMs - activeStartMs);

      fed4.resetTouchFlags();
      if (activePad == 1)
      {
        fed4.leftCount++;
        fed4.leftTouch = true;
      }
      else if (activePad == 2)
      {
        fed4.centerCount++;
        fed4.centerTouch = true;
      }
      else if (activePad == 3)
      {
        fed4.rightCount++;
        fed4.rightTouch = true;
      }

      fed4.logTouch("Poke");
      fed4.update(FedUpdateMode::Poke); // counters + L/C/R dots (MIP)

      Serial.printf("Poke pad=%d peak=%lu hold=%.0f ms\n", activePad,
                    (unsigned long)activePeakSmooth, fed4.pokeDuration);

      FED4::wakePad = 0;
      activePad = 0;
      activePeakSmooth = 0;
    }
  }

  // ── 1 Hz Heartbeat row ─────────────────────────────────────────────────────
  if (nowMs - lastHeartbeatMs >= SD_HEARTBEAT_MS)
  {
    lastHeartbeatMs = nowMs;
    fed4.logTouch("Heartbeat");
  }

  // ── Keep ENV / battery covariates fresh (no waitUntil() here to do it) ─────
  if (nowMs - lastSensorMs >= SENSOR_POLL_MS)
  {
    lastSensorMs = nowMs;
    fed4.refreshSensors();
    fed4.update();
  }

  // ── 10 Hz bench print ──────────────────────────────────────────────────────
  if (nowMs - lastPrintMs >= SERIAL_PRINT_MS)
  {
    lastPrintMs = nowMs;
    Serial.printf(
        "sm L:%lu C:%lu R:%lu | bm L:%lu C:%lu R:%lu | idle L:%lu C:%lu R:%lu | "
        "d-idle L:%ld C:%ld R:%ld | wakeAbs L:%lu C:%lu R:%lu\n",
        (unsigned long)smooth[0], (unsigned long)smooth[1], (unsigned long)smooth[2],
        (unsigned long)fed4TouchReadBenchmark(TOUCH_PAD_LEFT),
        (unsigned long)fed4TouchReadBenchmark(TOUCH_PAD_CENTER),
        (unsigned long)fed4TouchReadBenchmark(TOUCH_PAD_RIGHT),
        (unsigned long)fed4TouchIdleL, (unsigned long)fed4TouchIdleC,
        (unsigned long)fed4TouchIdleR,
        (long)((int32_t)smooth[0] - (int32_t)fed4TouchIdleL),
        (long)((int32_t)smooth[1] - (int32_t)fed4TouchIdleC),
        (long)((int32_t)smooth[2] - (int32_t)fed4TouchIdleR),
        (unsigned long)fed4TouchWakeAbsL, (unsigned long)fed4TouchWakeAbsC,
        (unsigned long)fed4TouchWakeAbsR);
  }

  delay(5);
}
