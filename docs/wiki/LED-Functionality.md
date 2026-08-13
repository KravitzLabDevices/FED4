# FED4 LED Functionality

The FED4 has **two LED systems**: a **front strip** of 8 NeoPixels around the nose-pokes, and a **single digital status LED** on `STATUS_LED`.

**Front strip (poke lights)**

- **`leftLight("red")`**, **`centerLight("green")`**, **`rightLight("blue")`** — light left/center/right poke; optional brightness: `leftLight("red", 100)`.
- **`setStripPixel(i, "green")`** — individual strip LED.
- **`colorWipe("white", 10)`**, **`stripRainbow(50, 1)`** — animations; **`lightsOff()`** — clear strip.

**Status LED**

- **`redPix(brightness)`** — on when brightness > 0 (digital; PWM path is reserved for MIP VCOM LEDC).
- **`noPix()`** — off.

**Strip colors:** `"red"`, `"green"`, `"blue"`, `"yellow"`, `"purple"`, `"cyan"`, `"orange"`, `"white"`.

See [FED4_LEDs.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_LEDs.cpp).
