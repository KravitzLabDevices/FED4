#ifndef FED4_DISPLAY_ORIENT_H
#define FED4_DISPLAY_ORIENT_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>

// Kyocera TN0216 portrait orientations (Adafruit_GFX rotation values).
static const uint8_t FED4_DISPLAY_ROTATION_NATIVE = 3;   // boot / X ≈ +1 g
static const uint8_t FED4_DISPLAY_ROTATION_FLIPPED = 1;  // 180° from native (X < 0)

static const float FED4_GRAVITY_MS2 = 9.80665f;

// Device X (left/right across screen) = -chip Y — see FED4_Accel.cpp.
inline float fed4DeviceAccelXG(const sensors_event_t &event) {
  return -event.acceleration.y / FED4_GRAVITY_MS2;
}

/** X axis carries gravity when the screen is edge-on to the ground.
 *  Native at startup when X ≥ 0; rotate 180° from native when X < 0. */
inline uint8_t fed4DisplayRotationForAccelX(float xG) {
  return (xG < 0.0f) ? FED4_DISPLAY_ROTATION_FLIPPED
                     : FED4_DISPLAY_ROTATION_NATIVE;
}

// Back-compat aliases
static const uint8_t FED4_DISPLAY_ROTATION_UPRIGHT = FED4_DISPLAY_ROTATION_NATIVE;
static const uint8_t FED4_DISPLAY_ROTATION_INVERTED = FED4_DISPLAY_ROTATION_FLIPPED;

#endif
