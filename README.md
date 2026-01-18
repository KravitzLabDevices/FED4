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

- Automated pellet dispensing system 
- Three nose-pokes
- Pellet approach sensor
- Multi-colored signal LEDs
- Audio feedback
- Haptic feedback
- Temperature/Humidity monitoring
- Activity monitoring
- Optional Hublink integration for network connectivity

## Hardware Components

- MCP23X17 I/O expander
- MAX17048 fuel gauge (battery monitor)
- DS3231 RTC (Real-Time Clock)
- BME680 temperature/humidity/pressure/gas sensor
- LIS3DH accelerometer
- MLX90393 magnetometer
- VL53L1X time-of-flight distance sensor
- STHS34PF80 motion sensor
- VEML7700 ambient light sensor
- MAX98357A I2S amplifier

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

## Usage Example

```cpp

#include <FED4.h>     // include the FED4 library
FED4 fed4;            // start FED4 object
char task[] = "FR1";  // give the task a unique name

void setup() {
  fed4.begin(task);  // initialize FED4 hardware
}

void loop() {
  fed4.run();  // run this once per loop

  if (fed4.leftTouch) {     // if left poke is touched
    fed4.lowBeep();         // 500hz 200ms beep
    fed4.leftLight("red");  // light LEDs around left poke red
    fed4.logData("Left");
    fed4.feed();  // feed one pellet, logging drop and retrieval
  }

  if (fed4.centerTouch) {  // if center poke is touched
    fed4.click();          // audio click stimulus
    fed4.hapticBuzz();
    fed4.centerLight("green");  // light LEDs around center poke green
    fed4.logData("Center");
  }

  if (fed4.rightTouch) {      // if right poke is touched
    fed4.click();             // audio click stimulus
    fed4.rightLight("blue");  // light LEDs around right poke blue
    fed4.logData("Right");
  }
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
