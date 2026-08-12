/*
 * Pulls shared submodule slave sources into this sketch build.
 * Arduino IDE compiles .cpp only from the sketch directory.
 */
#include "../SubmoduleState.cpp"
#include "../SubmoduleRtc.cpp"
#include "../SubmoduleCommands.cpp"
#include "../SubmoduleI2cSlaveEsp32.cpp"
