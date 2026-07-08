#include "FED4.h"

// Forward-declare the ToF sensor instance from FED4_Prox.cpp
extern SFEVL53L1X distanceSensor;

/******************************************************************************
 * FED4 Interrupt Subsystem
 *
 * Hardware model:
 *
 *   ToF_INT  ─┐
 *   RTC_INT  ─┤  (open-drain, pulled to 3.3V) ── OPEN_DRAIN_INT ──┐
 *   BAT_INT  ─┘                                                     ├─ 74LVC1G08 ── INT_OR
 *   ACCEL_INT1 ──────────────────── (push-pull, active LOW) ────────┘
 *
 *
 * The AND gate of two active-LOW signals is an active-LOW OR: INT_OR goes LOW
 * when ANY source fires, and stays LOW until EVERY asserting source is cleared.
 *
 * Public API:
 *   initializeInterrupts()         – one-time setup (called from begin())
 *   interruptPending()             – true when INT_OR is LOW (asserted)
 *   scanInterrupts()               – bitmask of ALL active sources (read-only)
 *   clearInterrupts(mask)          – clear latches for selected sources
 *   scanAndClearInterrupts()       – scan + clear + verify line released
 *   firstInterruptSource()         – highest-priority single source (ACCEL > TOF > RTC > BAT)
 *   getLastInterruptMask()         – mask auto-captured on GPIO wake
 *
 * Opt-in source enables:
 *   enableAccelInterrupt(thresh_g, duration)
 *   enableRTCAlarmInterrupt(alarmNum)
 *   enableBatteryAlert(minV, maxV)
 ******************************************************************************/

// ── LIS2DH12 register helpers ────────────────────────────────────────────────
// The Adafruit_LIS3DH library (register-compatible with LIS2DH12) does not
// expose raw register write/read publicly, so we use Wire directly.

static constexpr uint8_t LIS3DH_ADDR = I2C_ADDR_ACCEL; // LIS2DH12TR

static void lis3dh_writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(LIS3DH_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t lis3dh_readReg(uint8_t reg) {
    Wire.beginTransmission(LIS3DH_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)LIS3DH_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

// ── initializeInterrupts ─────────────────────────────────────────────────────

bool FED4::initializeInterrupts()
{
    // Configure INT_OR as input (active-LOW, no internal pull — external pull on schematic)
    pinMode(INT_OR, INPUT);

    // Register for GPIO light-sleep wake (active LOW).
    // esp_sleep_enable_gpio_wakeup() is already called in initializeButtons().
    esp_err_t err = gpio_wakeup_enable((gpio_num_t)INT_OR, GPIO_INTR_LOW_LEVEL);
    if (err != ESP_OK) {
        Serial.println("INT_OR: failed to enable GPIO wakeup");
        return false;
    }

    Serial.println("INT_OR interrupt line initialized (active LOW)");

    // Auto-configure accel INT1 for wake-on-move at a conservative threshold.
    // Returns true even if accel is absent (non-fatal for interrupt init).
    enableAccelInterrupt();

    return true;
}

// ── interruptPending ─────────────────────────────────────────────────────────

bool FED4::interruptPending()
{
    return digitalRead(INT_OR) == LOW;
}

// ── scanInterrupts ───────────────────────────────────────────────────────────
// Checks each source's status without clearing any latch.
// Returns a bitmask of FED4IntSource flags for every currently-asserted source.

uint8_t FED4::scanInterrupts()
{
    uint8_t mask = INT_SRC_NONE;

    // ToF (VL53L1X): checkForDataReady() returns true when distance data is ready
    // or when a threshold interrupt was triggered (relevant in autonomous mode).
    if (distanceSensor.checkForDataReady()) {
        mask |= INT_SRC_TOF;
    }

    // RTC (DS3231): alarm 1 or alarm 2 flag set
    if (rtc.alarmFired(1) || rtc.alarmFired(2)) {
        mask |= INT_SRC_RTC;
    }

    // Battery (MAX17048): any alert flag active in STATUS register
    if (maxlipo.isActiveAlert()) {
        mask |= INT_SRC_BATTERY;
    }

    // Accel (LIS2DH12): read INT1_SRC IA bit (reading the register also clears it
    // when latching is enabled via CTRL_REG5 LIR_INT1=1).
    // Note: this clears the latch as a side effect of the scan.
    uint8_t int1src = lis3dh_readReg(LIS3DH_REG_INT1SRC);
    if (int1src & 0x40) { // bit 6 = IA (Interrupt Active)
        mask |= INT_SRC_ACCEL;
    }

    return mask;
}

// ── clearInterrupts ──────────────────────────────────────────────────────────
// Clears the interrupt latch for each source indicated by `mask`.
// After clearing all asserting sources, INT_OR should return HIGH.

void FED4::clearInterrupts(uint8_t mask)
{
    if (mask & INT_SRC_TOF) {
        distanceSensor.clearInterrupt();
    }

    if (mask & INT_SRC_RTC) {
        rtc.clearAlarm(1);
        rtc.clearAlarm(2);
    }

    if (mask & INT_SRC_BATTERY) {
        // Clear all active alert flags in the STATUS register
        uint8_t flags = maxlipo.getAlertStatus();
        maxlipo.clearAlertFlag(flags);
    }

    if (mask & INT_SRC_ACCEL) {
        // Reading INT1_SRC clears the latch when LIR_INT1=1 in CTRL_REG5.
        accel.readAndClearInterrupt();
    }
}

// ── scanAndClearInterrupts ───────────────────────────────────────────────────
// Returns the bitmask of asserted sources, then clears all latched sources.
// Logs a warning if INT_OR remains LOW after clearing (e.g. PIR still active).

uint8_t FED4::scanAndClearInterrupts()
{
    uint8_t mask = scanInterrupts();

    if (mask != INT_SRC_NONE) {
        clearInterrupts(mask);

        // Allow time for open-drain lines to settle before checking release
        delayMicroseconds(50);

        if (interruptPending()) {
            Serial.println("INT_OR: line still asserted after clear — a source may need more time");
        }
    }

    lastInterruptMask = mask;
    return mask;
}

// ── firstInterruptSource ─────────────────────────────────────────────────────
// Returns the single highest-priority source (priority: ACCEL > TOF > RTC > BAT > MOTION).

FED4::FED4IntSource FED4::firstInterruptSource()
{
    uint8_t mask = scanInterrupts();
    if (mask & INT_SRC_ACCEL)   return INT_SRC_ACCEL;
    if (mask & INT_SRC_TOF)     return INT_SRC_TOF;
    if (mask & INT_SRC_RTC)     return INT_SRC_RTC;
    if (mask & INT_SRC_BATTERY) return INT_SRC_BATTERY;
    return INT_SRC_NONE;
}

// ── getLastInterruptMask ─────────────────────────────────────────────────────
// Returns the bitmask captured by wakeUp() after an INT_OR GPIO wake.
// Call after fed.sleep() returns to inspect what caused the wake.

uint8_t FED4::getLastInterruptMask()
{
    return lastInterruptMask;
}

// ── enableAccelInterrupt ─────────────────────────────────────────────────────
// Configures the LIS2DH12TR INT1 pin for inertial wake-on-move and routes it
// to the AND gate input (ACCEL_INT1).
//
// Register sequence (LIS2DH12 datasheet; register map identical to LIS3DH):
//   CTRL_REG6 (0x25) bit 1 (H_L): 1 = INT1 active LOW (matches AND-gate)
//   CTRL_REG5 (0x24) bit 3 (LIR_INT1): 1 = latch INT1 until INT1_SRC read
//   CTRL_REG3 (0x22) bit 6 (I1_AOI1): 1 = route interrupt 1 event to INT1 pin
//   INT1_CFG  (0x30): enable high-event detection on all axes (0x2A = XH|YH|ZH)
//   INT1_THS  (0x32): threshold (1 LSB = full_scale / 128; @2G: ~15.6 mg/LSB)
//   INT1_DUR  (0x33): minimum duration (ODR ticks)
//
// threshold_g : acceleration threshold in g (default 0.1g = ~100 mg)
// duration_count : number of ODR samples event must persist (default 0 = 1 ODR tick)

bool FED4::enableAccelInterrupt(float threshold_g, uint8_t duration_count)
{
    // Verify the accel responds — LIS2DH12 WHO_AM_I = 0x33 (same as LIS3DH)
    uint8_t whoami = lis3dh_readReg(LIS3DH_REG_WHOAMI);
    if (whoami != 0x33) {
        Serial.printf("enableAccelInterrupt: LIS2DH12 not found (WHO_AM_I=0x%02X)\n", whoami);
        return false;
    }

    // INT1 active LOW; route ACCEL_INT1 into AND gate as active-LOW
    uint8_t ctrl6 = lis3dh_readReg(LIS3DH_REG_CTRL6);
    ctrl6 |= 0x02; // bit 1 = H_L: 1 = active LOW
    lis3dh_writeReg(LIS3DH_REG_CTRL6, ctrl6);

    // Latch INT1 until INT1_SRC is read (prevents spurious re-assertion)
    uint8_t ctrl5 = lis3dh_readReg(LIS3DH_REG_CTRL5);
    ctrl5 |= 0x08; // bit 3 = LIR_INT1
    lis3dh_writeReg(LIS3DH_REG_CTRL5, ctrl5);

    // Route AOI/motion interrupt 1 to INT1 pin
    uint8_t ctrl3 = lis3dh_readReg(LIS3DH_REG_CTRL3);
    ctrl3 |= 0x40; // bit 6 = I1_AOI1
    lis3dh_writeReg(LIS3DH_REG_CTRL3, ctrl3);

    // Interrupt config: OR of any high event on X, Y, or Z (0x2A = XHIE|YHIE|ZHIE)
    lis3dh_writeReg(LIS3DH_REG_INT1CFG, 0x2A);

    // Threshold: at ±2G range, 1 LSB ≈ 15.625 mg.  THS = thresh / 0.015625
    // Clamp to 7-bit range [1..127]
    uint8_t ths = (uint8_t)(threshold_g / 0.015625f);
    if (ths < 1)   ths = 1;
    if (ths > 127) ths = 127;
    lis3dh_writeReg(LIS3DH_REG_INT1THS, ths);

    // Duration (minimum hold time)
    lis3dh_writeReg(LIS3DH_REG_INT1DUR, duration_count);

    // Clear any stale INT1_SRC latch from before configuration
    lis3dh_readReg(LIS3DH_REG_INT1SRC);

    Serial.printf("Accel interrupt enabled: threshold %.3fg (%d LSB), duration %d ticks\n",
                  threshold_g, ths, duration_count);
    return true;
}

// ── enableRTCAlarmInterrupt ──────────────────────────────────────────────────
// Configures the DS3231 SQW/INT pin as an interrupt output (INTCN=1) and clears
// any stale alarm flag for the selected alarm.
//
// NOTE: The alarm time must be programmed by the sketch before calling this
// function (e.g. rtc.setAlarm1(...)).  setAlarm1()/setAlarm2() also sets the
// A1IE/A2IE bit in CONTROL_REG, which is what actually arms the interrupt output.
//
// alarmNum: 1 or 2

bool FED4::enableRTCAlarmInterrupt(uint8_t alarmNum)
{
    if (alarmNum < 1 || alarmNum > 2) {
        Serial.println("enableRTCAlarmInterrupt: alarmNum must be 1 or 2");
        return false;
    }
    // Route INT/SQW pin to interrupt mode (stops square-wave output)
    rtc.writeSqwPinMode(DS3231_OFF);

    // Clear any pre-existing alarm flag so the line is not already asserted
    rtc.clearAlarm(alarmNum);

    // Disable the unused alarm to avoid spurious triggers
    rtc.disableAlarm(alarmNum == 1 ? 2 : 1);

    Serial.printf("RTC alarm %d interrupt output configured (set alarm time with setAlarm%d())\n",
                  alarmNum, alarmNum);
    return true;
}

// ── enableBatteryAlert ───────────────────────────────────────────────────────
// Programs the MAX17048 VALERT window and enables the ALRT pin (BAT_INT, open-drain).
// minVoltage / maxVoltage in volts (e.g. 3.0, 4.3).

bool FED4::enableBatteryAlert(float minVoltage, float maxVoltage)
{
    maxlipo.setAlertVoltages(minVoltage, maxVoltage);
    // Clear any pre-existing flags before enabling
    maxlipo.clearAlertFlag(maxlipo.getAlertStatus());
    Serial.printf("Battery alert enabled: %.2fV – %.2fV\n", minVoltage, maxVoltage);
    return true;
}
