<p align="center">
  <img src="https://github.com/KravitzLabDevices/FED4/blob/main/extras/images/FED4_minilogo.png?raw=true" alt="FED4 Logo" width="120"/>
</p>

# FED4

[![Version](https://img.shields.io/badge/Version-1.0.0-blue.svg)](https://github.com/KravitzLabDevices/FED4)

<p align="center">
  <img src="https://github.com/KravitzLabDevices/FED4/blob/main/extras/images/FED4.gif?raw=true" alt="FED4 Demo" width="400"/>
</p>

## Overview

FED4 (Feeding Experimentation Device) is an open-source device for training mice. The device provides comprehensive control over feeding experiments, environmental monitoring, and data collection.

## Features

- 🔄 Automated pellet dispensing system
- 📊 Real-time data logging
- 🌡️ Environmental monitoring
- 🔋 Power management
- 📱 Interactive display
- 🎵 Audio feedback
- 💾 SD card storage
- 🕹️ Touch and button controls
- 🌐 Optional Hublink integration for network connectivity

## Hardware Components

- Sharp Memory Display
- NeoPixel and LED strips
- DS3231 RTC (Real-Time Clock)
- LIS3DH Accelerometer
- Temperature/Humidity Sensor
- Battery Monitor
- Stepper Motor
- Touch Sensors
- Motion Sensor
- Speaker System

## Library Structure

![FED4 overview](https://github.com/KravitzLabDevices/FED4/blob/main/extras/images/FED4_overview.png?raw=true)

### Core Functions

#### Initialization
```cpp
bool begin(const char* programName)
```
- Main initialization function
- Sets up all hardware components
- Returns success/failure status

#### Main Loop
```cpp
void run()
```
- Updates time and display
- Prints status
- Manages sleep cycles

### Power Management

#### Sleep Control
```cpp
void sleep()
```
- Timer-based wake-up (6 seconds)
- Sensor polling on wake
- Touch/button handling

#### LDO Control
```cpp
bool initializeLDOs()
void LDO2_ON()
void LDO2_OFF()
void LDO3_ON()
void LDO3_OFF()
```
- Power rail management
- Component power states

### Feeding System

#### Core Feeding
```cpp
void feed()
void initFeeding()
void dispense()
void finishFeeding()
```
- Pellet dispensing control
- Status monitoring
- Error handling

#### Motor Control
```cpp
bool initializeMotor()
void releaseMotor()
void minorJamClear()
void majorJamClear()
void vibrateJamClear()
```
- Stepper motor management
- Jam detection and clearing
- Error recovery

### Sensor Systems

#### Touch Sensors
```cpp
bool initializeTouch()
void handleTouch()
void calibrateTouchSensors()
```
- Capacitive input handling
- Auto-calibration
- Event processing

#### Motion Detection
```cpp
bool initializeMotionSensor()
void configureMotionSensor()
bool isMotionDataReady()
```
- Presence detection
- Configurable sensitivity
- Status monitoring

#### Accelerometer
```cpp
bool initializeAccel()
void setAccelRange()
void setAccelPerformanceMode()
```
- 3-axis measurement
- Multiple range options
- Power/performance modes

### User Interface

#### Display
```cpp
bool initializeDisplay()
void updateDisplay()
void displayTask()
void displayEnvironmental()
```
- Status visualization
- Menu system
- Environmental data display

#### LED Control
```cpp
bool initializePixel()
bool initializeStrip()
void setPixColor()
```
- Visual feedback
- Status indication
- Custom patterns

#### Audio
```cpp
bool initializeSpeaker()
void playTone()
void soundSweep()
```
- Event feedback
- Custom sound patterns
- Multiple frequencies

### Data Management

#### SD Card
```cpp
bool initializeSD()
bool createMetaJson()
void logData()
```
- Configuration storage
- Data logging
- File management

#### RTC
```cpp
bool initializeRTC()
void updateRTC()
DateTime now()
```
- Time management
- Event timestamping
- Synchronization

### Environmental Monitoring
```cpp
bool initializeVitals()
float getBatteryVoltage()
float getTemperature()
float getHumidity()
```
- System vitals
- Environmental conditions
- Battery monitoring

## Usage Example

```cpp
#include "FED4.h"

FED4 fed;

void setup() {
  // Initialize FED4 with program name
  fed.begin("MyExperiment");
  
  // Configure settings
  fed.setMouseId("M001");
  fed.setSex("M");
  fed.setStrain("C57BL/6");
}

void loop() {
  // Main operation loop
  fed.run();
}
```

## Hublink Integration

The FED4 library includes optional Hublink integration for network connectivity and time synchronization.

### Basic Hublink Usage

```cpp
#include "FED4.h"

FED4 fed;

void setup() {
  // Enable Hublink functionality
  fed.useHublink = true;
  
  // Initialize FED4 (Hublink will be initialized automatically)
  fed.begin("MyExperiment");
}

void loop() {
  // Hublink sync happens automatically in run()
  fed.run();
}
```

### Excluding Hublink

To exclude Hublink functionality if it's not being used:

```cpp
#define FED4_EXCLUDE_HUBLINK
#include "FED4.h"

FED4 fed;

void setup() {
  fed.begin("MyExperiment");
}
```

For more details, see the [Hublink examples](examples/4_Hublink/) and the [official Hublink documentation](https://hublink.cloud/docs).

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.


## Acknowledgments

The FED4 library relies on code from Adafruit and Sparkfun, who invest significant time and money developing open-source hardware and software. Please support them!

## Creators of FED4 hardware
- Lex Kravitz (Concept and 3D design)  
- Paul Price (Electronics design)

## Authors of library code

- Lex Kravitz
- Matt Gaidica

## Support

For support, please open an issue in the GitHub repository or contact the authors directly. 

### Meta Data Configuration File (meta.json)
The SD card should include a meta.json file containing metadata for the device. We use [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) to parse the JSON file. This format is compatible with [https://hublink.cloud](https://hublink.cloud). Here is the base schema:

![image](https://github.com/user-attachments/assets/c4a5b3f2-3347-4b75-b2be-a023ba61648d)

```json
{
    "subject": {
        "id": "mouse001",
        "strain": "C57BL/6",
        "sex": "male"
    },
    "fed": {
        "program": "Classic"
    },
    "hublink": {
        "advertise": "FED4",
        "advertise_every": 300,
        "advertise_for": 30,
        "disable": false
    }
}
```

Here is an [example meta.json](https://github.com/KravitzLabDevices/FED4/blob/main/extras/meta.json_examples/meta.json) for download

These meta data will be parsed in Arduino and other languages as well for post-analysis. Other JSON resources:
- [ArduinoJson Assistant 7](https://arduinojson.org/v7/assistant/#/step1)
- [JSON to Graph Converter](https://jsonviewer.tools/editor)

Here are some common uses with FED4:
```cpp
String subjectId = getMetaValue("subject", "id"); // within library
String subjectId = fed.getMetaValue("subject", "id"); // within sketch
```

### Creating a meta.json file
You can create a meta.json file in most terminals using:

```bash
cd /path/to/your/SD
echo '{"subject":{"id":"mouse001","strain":"C57BL/6","sex":"male"},"fed":{"program":"Classic"}}' > meta.json
```

On Windows PowerShell:
```powershell
Set-Content -Path meta.json -Value '{"subject":{"id":"mouse001","strain":"C57BL/6","sex":"male"},"fed":{"program":"Classic"}}'
```

## Dependencies
This library requires several Arduino libraries for hardware interfacing. See [FED4.h](src/FED4.h) for a complete list of dependencies.

## License
This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

For more information about the FED project, visit [https://github.com/KravitzLab](https://github.com/KravitzLab)
