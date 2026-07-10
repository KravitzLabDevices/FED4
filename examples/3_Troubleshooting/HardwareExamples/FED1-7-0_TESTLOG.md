# FED v1.7 Hardware Examples Test Log

| SKETCH | PASS | FUNCTIONS | NOTES |
|--------|:----:|-----------|-------|
| `FED4-Accel-Adafruit.ino` | ✓ | LIS3DH accelerometer, main I2C bus, serial accel readout | |
| `FED4-Accel.ino` | | LIS3DH accelerometer, MIP display, tilt-controlled ball demo | |
| `FED4-Battery-Fuel-Guage.ino` | ✓ | MAX17048 fuel gauge, main I2C bus, voltage/percent monitoring | Passed; see note. Gauge on VBATT (not 3.3V). With USB and no LiPo, VBATT ~4.2V (MCP73831 + 100µF cap) — valid voltage/`isDeviceReady()`, invalid SOC, ~−4%/hr drain. USB removed: VBATT falls to ~1.5V then cap bleed. MCP73831 STAT pulsing = no pack. Misleading readings are a bench-only USB/no-pack artifact; off VBUS, normal pack operation should be fine. Confirm with real battery. |
| `FED4-Color-Chase-TouchPads-PhotoGate.ino` | | Front RGB strip, touch pads, center photogate, I2S speaker, MCP power rails | |
| `FED4-DeepSleep.ino` | | Deep sleep/wake, buttons, touch pads, photogate, timer, INT_OR GPIO, MCP PSV2/PSV3 | |
| `FED4-Display-Standalone.ino` | | Kyocera MIP display, SPI, MCP reset/backlight, PSV2/PSV3 rails | |
| `FED4-Display.ino` | | Kyocera MIP display via FED4 library, SPI, MCP backlight toggle | |
| `FED4-Haptic.ino` | | Haptic motor, MCP23017, PSV2 power rail | |
| `FED4-I2C-Scanner.ino` | | Main I2C bus scan, MCP23017, PSV2/TCA4307 for RTC | |
| `FED4-LED-Position-Test.ino` | | Front 8-LED strip index mapping, per-pixel colors, left/center/right groups | |
| `FED4-LEDs-Front.ino` | ✓ | Front RGB strip, MCP23017 PSV3, three buttons, LED animations | |
| `FED4-LUX.ino` | ✓ | VEML7700 ambient light sensor, main I2C bus, lux readout | |
| `FED4-Mario-SFX.ino` | | I2S speaker via FED4 library, Mario-style sound effects | |
| `FED4-Motor.ino` | | 4-wire stepper motor (pins 38/45/46/47), forward/reverse rotation | |
| `FED4-PhotoGates.ino` | | Four photogate GPIO inputs (center, left, right, pellet detector) | |
| `FED4-RTC.ino` | ✓ | DS3231 RTC, main I2C via PSV2/TCA4307, date/time readout | |
| `FED4-SD-Card.ino` | ✓ | SD card over SPI, mount, directory listing, read/write | |
| `FED4-Speaker-with-SD.ino` | | I2S amplifier, SD card MP3 playback, buttons, MCP PSV2/amp enable | |
| `FED4-Speaker.ino` | | I2S amplifier, MCP PSV2/amp enable, buttons, generated tones | |
| `FED4-TRRS.ino` | | TRRS audio jack inputs, front RGB strip group color feedback | |
| `FED4-Temp.ino` | | BME680 environmental sensor, main I2C bus, temp/humidity/pressure/gas | |
| `FED4-ToF.ino` | ✓ | VL53L1X time-of-flight distance sensor, main I2C bus | |
| `FED4-Touch-Light-Sleep-Multiple.ino` | | Three touch pads, ESP32 light sleep with touchpad wake | |
| `FED4-Touch-Pads-beeps.ino` | | Touch pads, I2S speaker beeps, status LED, MCP PSV2/PSV3 | |
| `FED4-Touch.ino` | ✓ | Three ESP32 touch pads, status NeoPixel color feedback | |
