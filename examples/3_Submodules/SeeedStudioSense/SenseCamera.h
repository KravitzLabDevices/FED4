#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../SubmoduleRtc.h"

bool senseCaptureImageDatetime(const SubmoduleDateTime *dt);
bool senseCaptureImageNow(char *outName, size_t outNameLen);
bool senseCaptureImageNamed(const char *basename, char *outName, size_t outNameLen);
bool senseCaptureImageById(uint16_t imageId);
bool senseRenameCapture(const char *fromName, const char *toName);
bool senseSdCardReady();
const char *senseCaptureLastError();

void senseSetCaptureDebug(bool enabled);
bool senseCaptureDebugEnabled();
