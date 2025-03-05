#include "FED4.h"

void FED4::hapticBuzz(uint8_t duration)
{
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
}

void FED4::hapticDoubleBuzz(uint8_t duration)
{
    for (int i = 0; i < 2; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration);
    }
}

void FED4::hapticTripleBuzz(uint8_t duration)
{
    for (int i = 0; i < 3; i++){
    mcp.digitalWrite(EXP_HAPTIC, HIGH);
    delay(duration);
    mcp.digitalWrite(EXP_HAPTIC, LOW);
    delay(duration);
    }
}