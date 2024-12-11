# FED4 Library
An Arduino library for the Feeding Experimentation Device version 4 (FED4), an open-source device for rodent behavioral experiments.

## Getting Started
### Meta Data Configuration File (meta.json)
The SD card should include a meta.json file containing metadata for the device. We use [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) to parse the JSON file. This format is compatible with [https://hublink.cloud](https://hublink.cloud). Here is the base schema:

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

These meta data will be parsed using the following prototype in Arduino, but can be parsed in other languages as well for post-analysis:

```cpp
// String input;
JsonDocument doc;

DeserializationError error = deserializeJson(doc, input);

if (error) {
  Serial.print("deserializeJson() failed: ");
  Serial.println(error.c_str());
  return;
}

JsonObject subject = doc["subject"];
const char* subject_id = subject["id"]; // "mouse001"
const char* subject_strain = subject["strain"]; // "C57BL/6"
const char* subject_sex = subject["sex"]; // "male"

const char* fed_program = doc["fed"]["program"]; // "Classic"
```

*Coded by [ArduinoJson Assistant 7](https://arduinojson.org/v7/assistant/#/step1).* There are a number of free JSON editors/visualizers (e.g., [JSON to Graph Converter](https://jsonviewer.tools/editor)).

### Creating a meta.json file
On unix-like systems, you can create a meta.json file using:
```bash
cd /path/to/your/SD
echo '{"subject":{"id":"mouse001","strain":"C57BL/6","sex":"male"},"fed":{"program":"Classic"}}' > meta.json
```

On Windows, you can create a meta.json file using:
```bash
cd /path/to/your/SD
echo '{"subject":{"id":"mouse001","strain":"C57BL/6","sex":"male"},"fed":{"program":"Classic"}}' > meta.json
```

## Dependencies
This library requires several Arduino libraries for hardware interfacing. See [FED4.h](src/FED4.h) for a complete list of dependencies.

## License
This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

For more information about the FED project, visit [https://github.com/KravitzLab](https://github.com/KravitzLab)