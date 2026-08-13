#include "FED4.h"

bool FED4::initializePower()
{
  mcp.pinMode(EXP_PSV2_EN, OUTPUT);
  mcp.pinMode(EXP_PSV3_EN, OUTPUT);
  PSV2_ON();
  PSV3_ON();
  return true;
}

void FED4::PSV2_ON()
{
  mcp.digitalWrite(EXP_PSV2_EN, LOW); // active LOW enable
}

void FED4::PSV2_OFF()
{
  mcp.digitalWrite(EXP_PSV2_EN, HIGH);
}

void FED4::PSV3_ON()
{
  mcp.digitalWrite(EXP_PSV3_EN, LOW); // active LOW enable
}

void FED4::PSV3_OFF()
{
  mcp.digitalWrite(EXP_PSV3_EN, HIGH);
}
