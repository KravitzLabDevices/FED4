# FED4 Wireless Capabilities and Hublink

The FED4 supports **wireless connectivity** via **Hublink**, which uses **BLE** to connect nodes (FED4) to a gateway, and the gateway to **[hublink.cloud](https://hublink.cloud)** for time sync, file uploads, and alerts. FED4 is **automatically configured for hublink.cloud** through the **`meta.json`** file on the SD card—no Hublink-specific code is required beyond enabling it.

**Automatic configuration (meta.json)**

- The **`meta.json`** schema is **compatible with hublink.cloud**. A **`hublink`** section configures BLE advertising and upload behavior:
  - **`advertise`** — BLE advertising name (e.g. `"FED4"`).
  - **`advertise_every`** — seconds between advertising periods (e.g. 300).
  - **`advertise_for`** — duration of each advertising period in seconds (e.g. 30).
  - **`upload_path`**, **`append_path`** — where files go in cloud storage (e.g. `/FED`, `subject:id`).
  - **`disable`** — set `true` to turn Hublink off.
- **`subject`**, **`device`**, etc. in **`meta.json`** are used for metadata and paths. The same file drives both FED4 metadata and Hublink; **one config, automatically applied** when Hublink is enabled.

**Enabling Hublink**

- Set **`fed.useHublink = true`** before **`fed.begin(...)`**. **`begin()`** then calls **`initializeHublink()`** (reads **`meta.json`**, starts Hublink). **`run()`** calls **`syncHublink()`** before sleep—**sync is automatic** each wake cycle.
- **`initializeHublink()`** — init Hublink (uses **SD_CS** pin); registers **`onHublinkTimestampReceived`** for RTC updates.
- **`syncHublink()`** — sets battery level, low-battery alerts, and runs **`hublink.sync()`** (advertise/connect, upload files, receive time).

**What runs automatically**

- **RTC time sync** — timestamps from Hublink update the RTC via **`adjustRTC()`**.
- **Low-battery alerts** — battery under 20% triggers **`hublink.setAlert("Low Battery!")`** before sync.
- **File uploads** — log files (and **`meta.json`**) are uploaded according to **`upload_path`** / **`append_path`** when the gateway is in range.

**Optional exclusion**

- **`#define FED4_EXCLUDE_HUBLINK`** before **`#include <FED4.h>`** removes Hublink from the build to save code size.

**Requirements:** Hublink library, SD card with **`meta.json`**, gateway in range for BLE, and internet for the gateway → hublink.cloud.

See [FED4_Hublink.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Hublink.cpp), [Hublink examples](https://github.com/KravitzLabDevices/FED4/tree/main/examples/4_Hublink), [meta.json example](https://github.com/KravitzLabDevices/FED4/blob/main/extras/meta.json_examples/meta.json). Full config and cloud usage: **[hublink.cloud/docs](https://hublink.cloud/docs)**.
