#!/usr/bin/env bash
#
# Cloud Agent / dev-environment bootstrap for the FED4 Arduino library.
#
# Installs the Arduino toolchain used to build the FED4 library and its
# example sketches for the ESP32-S3 target:
#   * arduino-cli
#   * the arduino-esp32 core, pinned to the version the project targets
#   * every third-party library FED4 depends on, at the pinned versions
#     recorded in src/FED4.h / library.properties
#   * a symlink that exposes this checkout to arduino-cli as the "FED4" library
#
# The script is idempotent: re-running it re-uses already-installed tools and
# libraries instead of failing.
set -euo pipefail

# --- Configuration ---------------------------------------------------------
# arduino-esp32 core version. Kept in sync with scripts/generate-clangd.sh
# (FED4_CLANGD_CORE) and the ESP-IDF 5.4 / core 3.2.x APIs used in src/.
ESP32_CORE_VERSION="${FED4_ESP32_CORE:-3.2.1}"
ESP32_INDEX_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

# Third-party libraries and the versions FED4 is developed against.
# Names are the exact arduino-cli registry names.
LIBRARIES=(
  "Adafruit MCP23017 Arduino Library@2.3.2"
  "Adafruit MAX1704X@1.0.3"
  "Stepper@1.1.3"
  "FastLED@3.10.2"
  # 1.12.6: transitive deps (e.g. Adafruit SSD1306) require >=1.12.6; the
  # library targets 1.12.3 but is source-compatible with this patch release.
  "Adafruit GFX Library@1.12.6"
  "RTClib@2.1.4"
  "Adafruit BME680 Library@2.0.5"
  "ArduinoJson@7.4.3"
  "Adafruit LIS3DH@1.3.0"
  "Adafruit Unified Sensor@1.1.15"
  "SparkFun VL53L1X 4m Laser Distance Sensor@1.2.12"
  "Adafruit VEML7700 Library@2.1.6"
  "ESP32Time@2.0.6"
  # Optional BLE sync (guarded by FED4_EXCLUDE_HUBLINK). Installed so the
  # full-featured build works out of the box.
  "Hublink-Node@1.0.12"
)

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="${HOME}/bin"

# --- Install arduino-cli ---------------------------------------------------
export PATH="${BIN_DIR}:${PATH}"
if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "==> Installing arduino-cli into ${BIN_DIR}"
  mkdir -p "${BIN_DIR}"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
    | BINDIR="${BIN_DIR}" sh
else
  echo "==> arduino-cli already installed: $(command -v arduino-cli)"
fi
arduino-cli version

# --- Configure board manager + package index ------------------------------
echo "==> Configuring arduino-cli"
arduino-cli config init --overwrite >/dev/null
arduino-cli config set board_manager.additional_urls "${ESP32_INDEX_URL}"
arduino-cli core update-index

# --- Install the ESP32 core ------------------------------------------------
echo "==> Installing esp32:esp32@${ESP32_CORE_VERSION}"
arduino-cli core install "esp32:esp32@${ESP32_CORE_VERSION}"

# --- Install library dependencies ------------------------------------------
echo "==> Installing FED4 library dependencies"
arduino-cli lib install "${LIBRARIES[@]}"

# --- Expose this checkout as the FED4 library ------------------------------
SKETCHBOOK="$(arduino-cli config get directories.user)"
LIB_LINK="${SKETCHBOOK}/libraries/FED4"
echo "==> Linking ${REPO_ROOT} -> ${LIB_LINK}"
mkdir -p "${SKETCHBOOK}/libraries"
ln -sfn "${REPO_ROOT}" "${LIB_LINK}"

echo "==> FED4 development environment ready."
echo "    Build an example, e.g.:"
echo "      arduino-cli compile --fqbn esp32:esp32:esp32s3 examples/1_Programs/BasicFED4"
