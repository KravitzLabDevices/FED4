#pragma once

#include "SubmoduleState.h"

// Handle one UART line (no trailing newline required). Returns true if a reply
// was written into replyOut (NUL-terminated, includes trailing \n when set).
bool submoduleHandleUartLine(SubmoduleState *state, const char *line,
                             char *replyOut, size_t replyOutLen);

// Rename last capture on SD. Board provides filesystem rename.
typedef bool (*SubmoduleRenameFn)(const char *fromName, const char *toName);

void submoduleSetRenameFn(SubmoduleRenameFn fn);
