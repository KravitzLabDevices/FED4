/*
 * FED4 Submodule Master Test — SeeedStudio Sense (TRIG + UART v1.0)
 *
 * Requires FED4_ENABLE_SUBMODULE 1 in src/FED4.h (library rebuild).
 * Companion slave: examples/3_Submodules/SeeedStudioSense/SeeedStudioSense.ino
 * Spec: examples/3_Submodules/README.md
 *
 * Wiring:
 *   FED4 TRRS2 (GPIO5 / TRIG) -> Sense GPIO1 (D0)
 *   FED4 TRRS3 (GPIO6 / DATA) -> Sense GPIO2 (D1)  [half-duplex + pull-up]
 *   3V3 / GND
 */

#include <FED4.h>

#if !FED4_ENABLE_SUBMODULE
#error "Set FED4_ENABLE_SUBMODULE to 1 in src/FED4.h and rebuild the library."
#endif

FED4 fed4;

void setup()
{
  fed4.begin("SenseTrigTest");
  Serial.println("=== FED4 Submodule Master Test (TRIG+UART) ===");

  fed4.senseBegin();
  if (fed4.senseSyncTime(3000))
  {
    Serial.println("senseSyncTime OK");
  }
  else
  {
    Serial.println("senseSyncTime FAILED");
  }
}

void loop()
{
  Serial.println("Pulse TRIG (10 ms)...");
  fed4.colorWipe("white", 0);
  fed4.senseTrigPulse(10);
  fed4.noPix();
  Serial.println("Pulse done (Sense captures async)");
  delay(5000);
}
