# FED4 Haptic Functionality

The FED4 drives a **haptic motor** via the MCP expander (pin **EXP_HAPTIC**). Used for vibration feedback on pokes, buttons, and events.

**Functions**

- **`hapticBuzz(duration)`** — single buzz (default 100 ms).
- **`hapticDoubleBuzz(duration)`** — two buzzes (default 25 ms each).
- **`hapticTripleBuzz(duration)`** — three buzzes (default 5 ms base; on/off = duration×10).
- **`hapticRumble(duration_ms)`** — low-frequency PWM rumble (default 300 ms, 50 Hz).

Durations are in milliseconds unless noted. See [FED4_Haptic.cpp](https://github.com/KravitzLabDevices/FED4/blob/main/src/FED4_Haptic.cpp).
