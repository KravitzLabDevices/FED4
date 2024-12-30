# FED4 Library
An Arduino library for the Feeding Experimentation Device version 4 (FED4), an open-source device for rodent behavioral experiments.

## Getting Started
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
``` cpp
String subjectId = getMetaValue("subject", "id"); // within library
String subjectId = fed.getMetaValue("subject", "id"); // within sketch
```

### Creating a meta.json file
You can create a meta.json file in most terminals using:

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
