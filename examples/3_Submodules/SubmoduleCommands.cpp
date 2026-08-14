#include "SubmoduleCommands.h"

#include <stdio.h>
#include <string.h>
#include "SubmoduleProtocol.h"
#include "SubmoduleRtc.h"

static SubmoduleRenameFn gRenameFn = nullptr;

void submoduleSetRenameFn(SubmoduleRenameFn fn) {
  gRenameFn = fn;
}

static void setReply(char *replyOut, size_t replyOutLen, const char *msg) {
  if (replyOut == nullptr || replyOutLen == 0) {
    return;
  }
  snprintf(replyOut, replyOutLen, "%s", msg);
}

bool submoduleHandleUartLine(SubmoduleState *state, const char *line,
                             char *replyOut, size_t replyOutLen) {
  if (state == nullptr || line == nullptr) {
    return false;
  }

  const char *p = line;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0') {
    return false;
  }

  if (*p == 'T' || *p == 't') {
    if (submoduleApplySetTimeLine(&state->rtcTime, &state->rtcValid, p)) {
      submoduleStateSetError(state, SUBMODULE_ERR_NONE);
      setReply(replyOut, replyOutLen, "OK\n");
      return true;
    }
    submoduleStateSetError(state, SUBMODULE_ERR_BAD_CMD);
    setReply(replyOut, replyOutLen, "ERR 1\n");
    return true;
  }

  if (*p == 'R' || *p == 'r') {
    p++;
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (*p == '\0' || state->lastFilename[0] == '\0' || gRenameFn == nullptr) {
      submoduleStateSetError(state, SUBMODULE_ERR_RENAME);
      setReply(replyOut, replyOutLen, "ERR 4\n");
      return true;
    }

    char newName[32];
    snprintf(newName, sizeof(newName), "%s", p);
    // Strip trailing CR/LF/spaces
    size_t n = strlen(newName);
    while (n > 0 &&
           (newName[n - 1] == '\r' || newName[n - 1] == '\n' ||
            newName[n - 1] == ' ')) {
      newName[--n] = '\0';
    }

    if (!gRenameFn(state->lastFilename, newName)) {
      submoduleStateSetError(state, SUBMODULE_ERR_RENAME);
      setReply(replyOut, replyOutLen, "ERR 4\n");
      return true;
    }

    snprintf(state->lastFilename, sizeof(state->lastFilename), "%s", newName);
    submoduleStateSetError(state, SUBMODULE_ERR_NONE);
    setReply(replyOut, replyOutLen, "OK\n");
    return true;
  }

  submoduleStateSetError(state, SUBMODULE_ERR_BAD_CMD);
  setReply(replyOut, replyOutLen, "ERR 1\n");
  return true;
}
