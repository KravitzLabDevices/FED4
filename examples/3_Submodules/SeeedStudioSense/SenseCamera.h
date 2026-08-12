#pragma once

#include "../SubmoduleRtc.h"

bool senseCaptureImageDatetime(const SubmoduleDateTime *dt);
bool senseCaptureImageById(uint16_t imageId);
bool senseSdCardReady();
const char *senseCaptureLastError();

void senseSetCaptureDebug(bool enabled);
bool senseCaptureDebugEnabled();
