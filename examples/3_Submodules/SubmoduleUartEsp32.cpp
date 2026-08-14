#include "SubmoduleUartEsp32.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "SubmoduleCommands.h"
#include "SubmoduleProtocol.h"

static HardwareSerial SubmoduleSerial(1);

bool submoduleUartEsp32Init(SubmoduleUartEsp32 *ctx,
                            const SubmoduleUartEsp32Config *config,
                            SubmoduleState *state) {
  if (ctx == nullptr || config == nullptr || state == nullptr) {
    return false;
  }
  ctx->config = *config;
  ctx->state = state;
  ctx->uartUp = false;
  ctx->capturing = false;

  pinMode(ctx->config.trigPin, INPUT_PULLUP);
  pinMode(ctx->config.dataPin, INPUT_PULLUP);
  return true;
}

void submoduleUartEsp32EndUart(SubmoduleUartEsp32 *ctx) {
  if (ctx == nullptr || !ctx->uartUp) {
    return;
  }
  SubmoduleSerial.end();
  pinMode(ctx->config.dataPin, INPUT_PULLUP);
  ctx->uartUp = false;
}

// Avoid same-pin RX+TX begin() — it can hang on ESP32-S3 without a solid
// one-wire pull-up/peer. Use RX-only or TX-only instead.
static void uartStartRxOnly(SubmoduleUartEsp32 *ctx) {
  SubmoduleSerial.end();
  pinMode(ctx->config.dataPin, INPUT_PULLUP);
  SubmoduleSerial.begin(ctx->config.baud, SERIAL_8N1, ctx->config.dataPin, -1);
  ctx->uartUp = true;
}

static void uartStartTxOnly(SubmoduleUartEsp32 *ctx) {
  SubmoduleSerial.end();
  SubmoduleSerial.begin(ctx->config.baud, SERIAL_8N1, -1, ctx->config.dataPin);
  ctx->uartUp = true;
}

bool submoduleUartEsp32BeginUart(SubmoduleUartEsp32 *ctx) {
  if (ctx == nullptr) {
    return false;
  }
  Serial.printf("UART begin RX-only: pin=%d baud=%lu\n", ctx->config.dataPin,
                (unsigned long)ctx->config.baud);
  Serial.flush();
  uartStartRxOnly(ctx);
  Serial.println("UART begin: done");
  Serial.flush();
  return true;
}

void submoduleUartEsp32SendRdy(SubmoduleUartEsp32 *ctx) {
  if (ctx == nullptr) {
    return;
  }
  Serial.println("UART: TX RDY");
  Serial.flush();
  uartStartTxOnly(ctx);
  SubmoduleSerial.print("RDY\n");
  delay(2);
  uartStartRxOnly(ctx);
  Serial.println("UART: back to RX");
  Serial.flush();
}

void submoduleUartEsp32EnterLightSleep(SubmoduleUartEsp32 *ctx) {
  if (ctx == nullptr) {
    return;
  }

  submoduleUartEsp32EndUart(ctx);

  gpio_wakeup_enable((gpio_num_t)ctx->config.trigPin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  Serial.flush();
  esp_light_sleep_start();

  gpio_wakeup_disable((gpio_num_t)ctx->config.trigPin);
}

static bool readLine(HardwareSerial &port, char *buf, size_t bufLen,
                     uint32_t timeoutMs) {
  if (buf == nullptr || bufLen < 2) {
    return false;
  }
  size_t n = 0;
  const uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    while (port.available()) {
      const char c = (char)port.read();
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

static void pollUartWhileTrigLow(SubmoduleUartEsp32 *ctx) {
  char line[96];
  char reply[32];
  uint32_t lastBeatMs = millis();
  uint32_t loops = 0;

  Serial.printf("UART poll: waiting while TRIG LOW (pin %d=%d)\n",
                ctx->config.trigPin, digitalRead(ctx->config.trigPin));
  Serial.println("  Release TRIG HIGH to finish session / re-sleep");
  Serial.flush();

  while (digitalRead(ctx->config.trigPin) == LOW) {
    loops++;
    if ((millis() - lastBeatMs) >= 1000) {
      lastBeatMs = millis();
      Serial.printf("UART poll heartbeat: still TRIG LOW (loops=%lu)\n",
                    (unsigned long)loops);
      Serial.flush();
    }
    if (ctx->uartUp && readLine(SubmoduleSerial, line, sizeof(line), 50)) {
      reply[0] = '\0';
      Serial.printf("UART RX: '%s'\n", line);
      Serial.flush();
      if (submoduleHandleUartLine(ctx->state, line, reply, sizeof(reply))) {
        uartStartTxOnly(ctx);
        SubmoduleSerial.print(reply);
        delay(2);
        uartStartRxOnly(ctx);
        Serial.printf("UART TX: %s", reply);
        Serial.flush();
      }
    }
    delay(1);
  }

  Serial.printf("UART poll: TRIG HIGH — leaving poll (loops=%lu)\n",
                (unsigned long)loops);
  Serial.flush();
}

static void runCapture(SubmoduleUartEsp32 *ctx, SubmoduleCaptureFn captureFn,
                       bool hadRtcAtWake) {
  ctx->capturing = true;
  Serial.printf("TRIG: capture start (rtcValid=%d hadRtc=%d)\n",
                (int)ctx->state->rtcValid, (int)hadRtcAtWake);
  Serial.flush();
  bool ok = false;
  if (captureFn != nullptr) {
    ok = captureFn(ctx->state);
  } else {
    Serial.println("TRIG: captureFn is null");
  }
  ctx->capturing = false;
  Serial.printf("TRIG: capture %s\n", ok ? "OK" : "FAIL");
  Serial.flush();
}

void submoduleUartEsp32HandleWakeSession(SubmoduleUartEsp32 *ctx,
                                         SubmoduleCaptureFn captureFn) {
  if (ctx == nullptr || ctx->state == nullptr) {
    Serial.println("HandleWakeSession: null ctx/state");
    Serial.flush();
    return;
  }

  const int trigLevel = digitalRead(ctx->config.trigPin);
  const bool hadRtcAtWake = ctx->state->rtcValid;

  Serial.printf("HandleWakeSession: enter rtcValid=%d TRIG=%d uartUp=%d\n",
                (int)hadRtcAtWake, trigLevel, (int)ctx->uartUp);
  Serial.flush();

  // No RTC yet:
  // - TRIG already HIGH (bench pulse) → capture only, skip UART
  // - TRIG LOW (FED4 holding for SET_TIME) → UART first, then debug capture if still unset
  if (!hadRtcAtWake) {
    if (trigLevel == HIGH) {
      Serial.println("No RTC + TRIG HIGH — bench capture, skip UART");
      Serial.flush();
      runCapture(ctx, captureFn, hadRtcAtWake);
      Serial.println("HandleWakeSession: done (bench path)");
      Serial.flush();
      return;
    }

    Serial.println("No RTC + TRIG LOW — UART for SET_TIME, then maybe capture");
    Serial.flush();
    if (submoduleUartEsp32BeginUart(ctx)) {
      submoduleUartEsp32SendRdy(ctx);
      pollUartWhileTrigLow(ctx);
      submoduleUartEsp32EndUart(ctx);
    }

    if (!ctx->state->rtcValid) {
      Serial.println("Still no RTC after UART — debug capture");
      Serial.flush();
      runCapture(ctx, captureFn, hadRtcAtWake);
    } else {
      Serial.println("SET_TIME ok this wake — skip capture");
      Serial.flush();
    }

    Serial.println("HandleWakeSession: done (pre-RTC UART path)");
    Serial.flush();
    return;
  }

  // RTC valid: normal async TRIG capture, then UART if TRIG still LOW.
  runCapture(ctx, captureFn, hadRtcAtWake);

  if (digitalRead(ctx->config.trigPin) == LOW) {
    if (submoduleUartEsp32BeginUart(ctx)) {
      submoduleUartEsp32SendRdy(ctx);
      pollUartWhileTrigLow(ctx);
      submoduleUartEsp32EndUart(ctx);
    }
  }

  Serial.println("HandleWakeSession: done");
  Serial.flush();
}
