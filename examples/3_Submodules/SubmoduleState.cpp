#include "SubmoduleState.h"

#include "SubmoduleProtocol.h"
#include <string.h>

void submoduleStateInit(SubmoduleState *state) {
  if (state == nullptr) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->lastErrorCode = SUBMODULE_ERR_NONE;
}

void submoduleStateSetError(SubmoduleState *state, uint8_t code) {
  if (state == nullptr) {
    return;
  }
  state->lastErrorCode = code;
}
