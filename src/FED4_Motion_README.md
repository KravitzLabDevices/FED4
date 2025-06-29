# FED4 Motion Sensor Module

This module adds motion detection capabilities to the FED4 library using the STHS34PF80 presence/motion sensor.

## Hardware Requirements

- FED4 device
- STHS34PF80 motion sensor connected to I2C_2 bus:
  - SDA: Pin 20
  - SCL: Pin 19
- Motion sensor powered by LDO2 (Pin 47)

## Library Dependencies

- FED4 library
- SparkFun STHS34PF80 Arduino Library

## Functions

### `bool initializeMotion()`

Initializes the motion sensor with optimal settings for motion detection.

**Returns:** `true` if initialization successful, `false` otherwise

**Features:**
- Automatic power management (enables LDO2 if needed)
- I2C connectivity testing
- Sensor configuration with recommended settings:
  - Data rate: 30Hz
  - Gain mode: Default
  - Low-pass filter: ODR/20
  - Motion threshold: 200
  - Motion hysteresis: 10

### `bool isMotionDetected()`

Checks if motion is currently detected by the sensor.

**Returns:** `true` if motion detected, `false` if no motion or error

**Features:**
- Automatic data ready checking
- Error handling for communication issues
- Simple boolean return for easy integration

## Usage Example

```cpp
#include "FED4.h"

FED4 fed4;

void setup() {
  // Initialize FED4 (motion sensor will be initialized automatically)
  fed4.begin("MyProgram");
}

void loop() {
  // Check for motion
  if (fed4.isMotionDetected()) {
    Serial.println("Motion detected!");
    // Your motion response code here
  }
  
  delay(100); // Small delay recommended
}
```

## Configuration

The motion sensor is configured with sensible defaults for general motion detection:

- **Data Rate:** 30Hz - Good balance between responsiveness and power consumption
- **Motion Threshold:** 200 - Moderate sensitivity, adjust based on your needs
- **Hysteresis:** 10 - Prevents rapid on/off switching
- **Low-pass Filter:** ODR/20 - Reduces noise while maintaining responsiveness

## Troubleshooting

### Common Issues

1. **"Motion sensor not found at address 0x5A"**
   - Check I2C connections (pins 20, 19)
   - Ensure LDO2 is enabled (pin 47)
   - Verify sensor is properly powered

2. **"Failed to initialize motion sensor"**
   - Check SparkFun STHS34PF80 library is installed
   - Verify I2C bus is working
   - Try power cycling the device

3. **No motion detection**
   - Check motion threshold settings
   - Ensure sensor has clear line of sight
   - Verify sensor is not too close or too far from target

### Debug Information

The initialization process provides detailed debug output including:
- Power status
- I2C connectivity test results
- Configuration step results
- Basic functionality test

## Integration Notes

- The motion sensor uses the second I2C bus (I2C_2) to avoid conflicts
- Power is managed through LDO2, which is enabled during FED4 initialization
- The sensor is automatically initialized when `fed4.begin()` is called
- Status is reported in the initialization summary

## Example Files

- `FED4_Motion_Example.ino` - Complete example showing motion detection with display feedback 