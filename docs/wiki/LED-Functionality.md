# FED4 LED Functionality

The FED4 has **two LED systems**: a **front strip** of 8 NeoPixels around the nose-pokes, and a **single status NeoPixel**.

**Front strip (poke lights)**

- **`leftLight("red")`**, **`centerLight("green")`**, **`rightLight("blue")`** — light left/center/right poke; optional brightness: `leftLight("red", 100)`.
- **`setStripPixel(i, "green")`** — individual strip LED.
- **`colorWipe("white", 10)`**, **`stripRainbow(50, 1)`** — animations; **`lightsOff()`** — clear strip.

**Status pixel**

- **`redPix(5)`**, **`greenPix(5)`**, **`bluePix(5)`**, **`noPix()`**, etc.

**Colors:** `"red"`, `"green"`, `"blue"`, `"yellow"`, `"purple"`, `"cyan"`, `"orange"`, `"white"`.

See [FED4_LEDs.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_LEDs.cpp).
