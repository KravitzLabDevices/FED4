# FED4 ToF Sensor Integration

This document describes the integration of the VL53L1X Time-of-Flight sensor into the FED4 library.

## Overview

The ToF sensor integration provides distance measurement capabilities to the FED4 device. The sensor is controlled via the MCP23017 GPIO expander and communicates over I2C.

## Hardware Setup

### Connections
- **I2C Connection**: Connect the VL53L1X sensor to the primary I2C bus (SDA: pin 8, SCL: pin 9)
- **XSHUT Pin**: Connect the sensor's XSHUT pin to the MCP expander pin `EXP_XSHUT_1` (pin 2)
- **Power**: The sensor is powered through the LDO2 regulator

### Pin Definitions
The ToF sensor uses the following pins defined in `FED4_Pins.h`:
- `EXP_XSHUT_1` (pin 2 on MCP expander) - Controls sensor power and I2C address

## Library Requirements

Add the following library to your Arduino IDE:
- **SparkFun VL53L1X Arduino Library** - Available through Library Manager

## API Reference

### Initialization
```cpp
bool initializeToF();
```
Initializes the ToF sensor. Returns `true` if successful, `false` otherwise.

### Reading Distance
```cpp
int readToFDistance();
```
Takes a distance measurement and returns the result in millimeters. Returns `-1` if the measurement failed.

### Printing Distance
```cpp
void printToFDistance();
```
Takes a distance measurement and prints the result to Serial output in the format:
```
ToF Distance(mm): [distance]
```

## Usage Example

```cpp
#include "FED4.h"

FED4 fed4;

void setup() {
  // Initialize FED4 (ToF sensor is automatically initialized)
  fed4.begin("MyProgram");
}

void loop() {
  // Take a distance measurement
  int distance = fed4.readToFDistance();
  
  if (distance >= 0) {
    Serial.printf("Distance: %d mm\n", distance);
  } else {
    Serial.println("ToF sensor error");
  }
  
  delay(1000);
}
```

## Integration Details

### Files Modified
1. **FED4.h** - Added function declarations and library include
2. **FED4_ToF.cpp** - New implementation file with sensor functions
3. **FED4_Begin.cpp** - Added ToF sensor initialization to startup sequence

### Initialization Sequence
The ToF sensor is initialized during the FED4 `begin()` function:
1. MCP expander is configured
2. XSHUT pin is set HIGH to enable the sensor
3. I2C communication is established
4. Sensor is configured and tested

### Error Handling
- Returns `-1` for failed measurements
- Initialization failures are reported in the startup status
- Serial output indicates sensor errors

## Troubleshooting

### Common Issues
1. **Sensor not found**: Check I2C connections and XSHUT pin
2. **Invalid readings**: Ensure sensor has clear line of sight
3. **Initialization failures**: Verify power supply and I2C address

### Debug Output
The sensor provides status messages during initialization:
- "Initializing ToF sensor..."
- "ToF sensor online!" (success)
- "ToF sensor failed to begin. Please check wiring." (failure)

## Technical Notes

- The sensor uses the SparkFun VL53L1X library for communication
- Distance measurements are in millimeters
- The sensor has a typical range of 0-4000mm
- Measurement time is approximately 30ms per reading
- The sensor automatically handles ambient light rejection 