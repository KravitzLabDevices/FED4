#include "FED4.h"

const uint8_t FED4::PIR_ACTIVITY_RING;
const uint32_t FED4::PIR_SAMPLE_MS;

// PIR activity.
// GPIO (PIR_MOTION) is the live EKMB1107112 level. STATUS_LED mirrors it.
// The S-35770E01 at I2C 0x32 counts rising edges, including while the ESP32
// sleeps. The library never resets that chip. It stores the raw value seen
// at begin() and only uses differences.
//
// One rising edge marks a detection. During the ~2.6 s standby hold the
// output can rise again, so extra edges in one gap are not extra activity.
// Samples are about every 4 s (PIR_SAMPLE_MS), including brief wakes from
// light sleep. A gap with any new edges credits min(gap, pirHoldSeconds).
// A gap with none credits 0. The 1.3 s mask is not credit and is not removed
// from the window. Activity stays unpublished until the scored span is at
// least one sample gap. Two visits inside one unread gap still count once.

static const uint32_t PIR_COUNT_MOD = 0x01000000u;

static bool readPirRaw(uint32_t &count)
{
    uint8_t got = Wire.requestFrom(I2C_ADDR_PIR_COUNTER, (uint8_t)3);
    if (got != 3) {
        return false;
    }
    uint32_t msb = Wire.read();
    uint32_t mid = Wire.read();
    uint32_t lsb = Wire.read();
    count = (msb << 16) | (mid << 8) | lsb;
    return true;
}

void FED4::recordPirSample(uint32_t nowMs)
{
    if (pirRingCount > 0) {
        const uint8_t last = (pirRingNext + PIR_ACTIVITY_RING - 1) % PIR_ACTIVITY_RING;
        if ((nowMs - pirRingMs[last]) < PIR_SAMPLE_MS) {
            return;
        }
    }
    pirRingMs[pirRingNext] = nowMs;
    pirRingTotal[pirRingNext] = pirTotal;
    pirRingNext = (pirRingNext + 1) % PIR_ACTIVITY_RING;
    if (pirRingCount < PIR_ACTIVITY_RING) {
        pirRingCount++;
    }
}

void FED4::refreshActivityPercent(uint32_t nowMs)
{
    if (pirRingCount == 0) {
        return;
    }

    const uint32_t windowMs = activityWindowSeconds * 1000UL;
    const uint32_t holdMs = (pirHoldSeconds > 0.0f)
                                ? (uint32_t)(pirHoldSeconds * 1000.0f)
                                : 0;

    uint8_t order[PIR_ACTIVITY_RING];
    for (uint8_t i = 0; i < pirRingCount; i++) {
        order[i] = (pirRingNext + PIR_ACTIVITY_RING - pirRingCount + i) % PIR_ACTIVITY_RING;
    }

    uint32_t activeMs = 0;
    uint32_t spanned = 0;
    for (uint8_t step = 0; step < pirRingCount; step++) {
        const bool tail = (step + 1 == pirRingCount);
        const uint8_t i0 = order[step];
        const uint32_t t0 = pirRingMs[i0];
        const uint32_t total0 = pirRingTotal[i0];
        const uint32_t t1 = tail ? nowMs : pirRingMs[order[step + 1]];
        const uint32_t total1 = tail ? pirTotal : pirRingTotal[order[step + 1]];
        if (t1 == t0) {
            continue;
        }
        if (windowMs > 0 && (nowMs - t1) >= windowMs) {
            continue;
        }

        uint32_t start = t0;
        if (windowMs > 0 && (nowMs - t0) > windowMs) {
            start = nowMs - windowMs;
        }
        const uint32_t inWindow = t1 - start;
        if (inWindow == 0) {
            continue;
        }
        spanned += inWindow;
        if ((total1 - total0) > 0 && holdMs > 0) {
            activeMs += (inWindow < holdMs) ? inWindow : holdMs;
        }
    }

    if (spanned < PIR_SAMPLE_MS) {
        activityReady = false;
        return;
    }
    float pct = (float)activeMs / (float)spanned * 100.0f;
    // A full hold in every 4 s gap is 2.6/4. Stretch that ceiling to 100.
    if (holdMs > 0) {
        pct *= (float)PIR_SAMPLE_MS / (float)holdMs;
    }
    if (pct > 100.0f) {
        pct = 100.0f;
    }
    activityPercent = pct;
    activityReady = true;
}

bool FED4::initializePirCounter()
{
    pirCounterAvailable = false;
    activityPercent = 0.0f;
    activityReady = false;
    pirTotal = 0;
    pirLastRaw = 0;
    pirRingNext = 0;
    pirRingCount = 0;

    if (!useMotionSensor) {
        return false;
    }
    if (!i2cProbe(I2C_ADDR_PIR_COUNTER)) {
        return false;
    }
    uint32_t raw = 0;
    if (!readPirRaw(raw)) {
        return false;
    }
    // Baseline only. Edges already in the chip are not part of this session.
    pirLastRaw = raw;
    pirCounterAvailable = true;
    return true;
}

bool FED4::updateMotionCount()
{
    if (!pirCounterAvailable) {
        return false;
    }
    uint32_t raw = 0;
    if (!readPirRaw(raw)) {
        return false;
    }
    const uint32_t delta = (raw - pirLastRaw) & (PIR_COUNT_MOD - 1);
    pirTotal += delta;
    pirLastRaw = raw;

    const uint32_t nowMs = millis();
    recordPirSample(nowMs);
    refreshActivityPercent(nowMs);
    return true;
}

void FED4::updateStatusLedFromMotion()
{
    if (!useMotionSensor) {
        return;
    }
    // Digital only — do not use analogWrite (conflicts with VCOM LEDC clock)
    digitalWrite(STATUS_LED, digitalRead(PIR_MOTION) ? HIGH : LOW);
}

bool FED4::motion()
{
    if (!useMotionSensor) {
        return false;
    }

    motionDetected = digitalRead(PIR_MOTION) == HIGH;
    updateStatusLedFromMotion();
    updateMotionCount();
    return motionDetected;
}

void FED4::resetMotionCounters()
{
    activityPercent = 0.0f;
    activityReady = false;
    pirRingNext = 0;
    pirRingCount = 0;
}
