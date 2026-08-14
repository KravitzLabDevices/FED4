#include "FED4.h"

// TRRS TRIG + UART Sense master.
// Spec: examples/3_Submodules/README.md
// Gated by FED4_ENABLE_SUBMODULE in FED4.h.

#if FED4_ENABLE_SUBMODULE

#include <HardwareSerial.h>
#include <string.h>

namespace {
HardwareSerial SenseSerial(1);
bool gSenseBegun = false;

bool senseReadLine(char *buf, size_t bufLen, uint32_t timeoutMs) {
  if (buf == nullptr || bufLen < 2) {
    return false;
  }
  size_t n = 0;
  const uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    while (SenseSerial.available()) {
      const char c = (char)SenseSerial.read();
      if (c == '\n' || c == '\r') {
        if (n > 0) {
          buf[n] = '\0';
          return true;
        }
        continue;
      }
      if (n + 1 < bufLen) {
        buf[n++] = c;
      }
    }
    delay(1);
  }
  if (n > 0) {
    buf[n] = '\0';
    return true;
  }
  return false;
}

void senseFlushRx() {
  while (SenseSerial.available()) {
    (void)SenseSerial.read();
  }
}
} // namespace

bool FED4::senseBegin() {
  pinMode(AUDIO_TRRS_2, OUTPUT);
  digitalWrite(AUDIO_TRRS_2, HIGH); // TRIG idle HIGH

  // Half-duplex one-wire on TRRS3
  SenseSerial.end();
  SenseSerial.begin(115200, SERIAL_8N1, AUDIO_TRRS_3, AUDIO_TRRS_3);
  gSenseBegun = true;
  return true;
}

void FED4::senseTrig(bool active) {
  if (!gSenseBegun) {
    senseBegin();
  }
  digitalWrite(AUDIO_TRRS_2, active ? LOW : HIGH);
}

void FED4::senseTrigPulse(uint32_t durationMs) {
  if (!gSenseBegun) {
    senseBegin();
  }
  digitalWrite(AUDIO_TRRS_2, LOW);
  delay(durationMs > 0 ? durationMs : 1);
  digitalWrite(AUDIO_TRRS_2, HIGH);
}

bool FED4::senseSyncTime(uint32_t timeoutMs) {
  if (!gSenseBegun) {
    senseBegin();
  }

  senseFlushRx();
  digitalWrite(AUDIO_TRRS_2, LOW);

  char line[64];
  const uint32_t t0 = millis();
  bool gotRdy = false;
  while ((millis() - t0) < timeoutMs) {
    if (senseReadLine(line, sizeof(line), 100)) {
      if (strncmp(line, "RDY", 3) == 0) {
        gotRdy = true;
        break;
      }
    }
  }

  if (!gotRdy) {
    digitalWrite(AUDIO_TRRS_2, HIGH);
    Serial.println("Sense: RDY timeout");
    return false;
  }

  DateTime t = now();
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "T %u %u %u %u %u %u\n", (unsigned)t.year(),
           (unsigned)t.month(), (unsigned)t.day(), (unsigned)t.hour(),
           (unsigned)t.minute(), (unsigned)t.second());
  SenseSerial.print(cmd);
  SenseSerial.flush();

  bool ok = false;
  const uint32_t t1 = millis();
  while ((millis() - t1) < timeoutMs) {
    if (senseReadLine(line, sizeof(line), 100)) {
      if (strncmp(line, "OK", 2) == 0) {
        ok = true;
        break;
      }
      if (strncmp(line, "ERR", 3) == 0) {
        Serial.printf("Sense: SET_TIME %s\n", line);
        break;
      }
    }
  }

  digitalWrite(AUDIO_TRRS_2, HIGH);
  if (!ok) {
    Serial.println("Sense: SET_TIME failed or timeout");
  }
  return ok;
}

#endif // FED4_ENABLE_SUBMODULE
