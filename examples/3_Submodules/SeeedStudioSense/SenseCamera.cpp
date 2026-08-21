#include "SenseCamera.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <stdarg.h>
#include <string.h>
#include <esp_camera.h>
#include "camera_pins.h"

// --- Camera color / exposure tuning (OV3660) ---
// White balance mode (set_wb_mode): 0=Auto, 1=Sunny, 2=Cloudy, 3=Office, 4=Home
static const int SENSE_WB_MODE = 3;
static const int SENSE_AEC_VALUE = 300;
static const int SENSE_AGC_GAIN = 0;

// No warmup discard — fixed AE/WB (TRIG wake path).
static const int SENSE_WARMUP_FRAMES = 0;

static char g_lastCaptureError[64] = "";
static bool g_captureDebug = true;

static void setCaptureError(const char *msg) {
  strncpy(g_lastCaptureError, msg, sizeof(g_lastCaptureError) - 1);
  g_lastCaptureError[sizeof(g_lastCaptureError) - 1] = '\0';
}

static void setCaptureErrorCode(const char *msg, esp_err_t err) {
  snprintf(g_lastCaptureError, sizeof(g_lastCaptureError), "%s (0x%x)", msg,
           (unsigned)err);
}

const char *senseCaptureLastError() {
  return g_lastCaptureError;
}

void senseSetCaptureDebug(bool enabled) {
  g_captureDebug = enabled;
}

bool senseCaptureDebugEnabled() {
  return g_captureDebug;
}

static void logCapture(const char *msg) {
  if (g_captureDebug) {
    Serial.println(msg);
  }
}

static void logCapturef(const char *fmt, ...) {
  if (!g_captureDebug) {
    return;
  }
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

static const char *sdCardTypeName(uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "NONE";
  }
}

static bool mountSdBus(bool logSteps) {
  if (logSteps) {
    logCapturef("CAPTURE: mounting SD (CS=%d SCK=%d MISO=%d MOSI=%d)...",
                SD_CS_PIN, SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  }

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ)) {
    setCaptureError("SD mount failed");
    if (logSteps) {
      logCapture("CAPTURE: SD.begin() failed");
    }
    return false;
  }

  const uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    SD.end();
    setCaptureError("SD card not detected");
    if (logSteps) {
      logCapture("CAPTURE: SD card type NONE");
    }
    return false;
  }

  const uint64_t cardSize = SD.cardSize();
  if (cardSize == 0) {
    SD.end();
    setCaptureError("SD card unreadable");
    if (logSteps) {
      logCapture("CAPTURE: SD card size is 0");
    }
    return false;
  }

  if (logSteps) {
    logCapturef("CAPTURE: SD ready (%s, %llu MB)", sdCardTypeName(cardType),
                (unsigned long long)(cardSize / (1024ULL * 1024ULL)));
  }
  return true;
}

bool senseSdCardReady() {
  const bool ok = mountSdBus(false);
  if (ok) {
    SD.end();
  }
  return ok;
}

static void tuneSensor(sensor_t *sensor) {
  if (sensor == nullptr) {
    return;
  }

  logCapturef("CAPTURE: sensor PID 0x%04x", (unsigned)sensor->id.PID);

  if (sensor->id.PID != OV3660_PID) {
    return;
  }

  // Orientation (Seeed OV3660 is mounted upside-down)
  sensor->set_vflip(sensor, 1);
  // sensor->set_hmirror(sensor, 0);  // 0=off, 1=flip horizontal

  // White balance — office default for FED4 chamber / strip lighting
  sensor->set_whitebal(sensor, 1);
  sensor->set_awb_gain(sensor, 1);
  sensor->set_wb_mode(sensor, SENSE_WB_MODE);

  // Fixed exposure / gain (minimize startup settle)
  sensor->set_exposure_ctrl(sensor, 0);  // manual AEC
  sensor->set_aec_value(sensor, SENSE_AEC_VALUE);
  sensor->set_gain_ctrl(sensor, 0);  // manual AGC
  sensor->set_agc_gain(sensor, SENSE_AGC_GAIN);

  // Color / tone (Espressif OV3660 starting point)
  sensor->set_brightness(sensor, 1);   // -2 .. +2
  sensor->set_saturation(sensor, -2);  // -2 .. +2

  logCapturef("CAPTURE: fixed AE=%d AGC=%d WB mode %d", SENSE_AEC_VALUE,
              SENSE_AGC_GAIN, SENSE_WB_MODE);
}

static void discardWarmupFrames() {
  for (int i = 0; i < SENSE_WARMUP_FRAMES; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
      logCapturef("CAPTURE: warmup frame %d failed", i + 1);
      break;
    }
    esp_camera_fb_return(fb);
  }
  if (SENSE_WARMUP_FRAMES > 0) {
    logCapturef("CAPTURE: discarded %d warmup frame(s)", SENSE_WARMUP_FRAMES);
  }
}

static bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    logCapture("CAPTURE: PSRAM found");
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    logCapture("CAPTURE: no PSRAM — using DRAM frame buffer");
  }

  logCapture("CAPTURE: initializing camera...");
  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    setCaptureErrorCode("camera init failed", err);
    logCapturef("CAPTURE: camera init failed 0x%x", (unsigned)err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    tuneSensor(sensor);
    discardWarmupFrames();
  } else {
    logCapture("CAPTURE: warning — sensor handle is null");
  }

  logCapture("CAPTURE: camera ready");
  return true;
}

static void shutdownCamera() {
  esp_camera_deinit();
}

static void shutdownSd() {
  SD.end();
  digitalWrite(SD_CS_PIN, HIGH);
}

static bool writeJpeg(const char *path) {
  logCapturef("CAPTURE: grabbing frame -> %s", path);

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb == nullptr) {
    setCaptureError("camera frame capture failed");
    logCapture("CAPTURE: esp_camera_fb_get() returned null");
    return false;
  }

  logCapturef("CAPTURE: frame %u bytes", (unsigned)fb->len);

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    setCaptureError("SD file open failed");
    logCapturef("CAPTURE: SD.open(%s) failed", path);
    return false;
  }

  const size_t frameLen = fb->len;
  const size_t written = file.write(fb->buf, frameLen);
  file.close();
  esp_camera_fb_return(fb);

  if (written != frameLen) {
    setCaptureError("SD write incomplete");
    logCapturef("CAPTURE: wrote %u of %u bytes", (unsigned)written,
                (unsigned)frameLen);
    return false;
  }

  logCapturef("CAPTURE: saved %u bytes", (unsigned)written);
  return true;
}

static bool captureToFile(const char *path) {
  g_lastCaptureError[0] = '\0';
  logCapturef("CAPTURE: begin -> %s", path);

  if (!mountSdBus(true)) {
    return false;
  }

  if (!initCamera()) {
    shutdownSd();
    return false;
  }

  const bool ok = writeJpeg(path);
  shutdownSd();
  shutdownCamera();

  if (ok) {
    g_lastCaptureError[0] = '\0';
    logCapture("CAPTURE: success");
  } else if (g_lastCaptureError[0] == '\0') {
    setCaptureError("unknown capture failure");
  }

  return ok;
}

bool senseCaptureImageDatetime(const SubmoduleDateTime *dt) {
  if (dt == nullptr) {
    setCaptureError("invalid datetime");
    return false;
  }

  char path[32];
  snprintf(path, sizeof(path), "/%04u%02u%02u%02u%02u%02u.jpg",
           (unsigned)dt->year, (unsigned)dt->month, (unsigned)dt->day,
           (unsigned)dt->hour, (unsigned)dt->min, (unsigned)dt->sec);
  return captureToFile(path);
}

bool senseCaptureImageNow(char *outName, size_t outNameLen) {
  SubmoduleDateTime now = {};
  if (!submoduleGetCurrentDateTime(&now)) {
    setCaptureError("system clock unreadable");
    return false;
  }

  char name[32];
  submoduleFormatDatetimeFilename(name, sizeof(name), &now);
  char path[36];
  snprintf(path, sizeof(path), "/%s", name);

  if (!captureToFile(path)) {
    return false;
  }

  if (outName != nullptr && outNameLen > 0) {
    snprintf(outName, outNameLen, "%s", name);
  }
  return true;
}

bool senseCaptureImageNamed(const char *basename, char *outName, size_t outNameLen) {
  if (basename == nullptr || basename[0] == '\0') {
    setCaptureError("invalid filename");
    return false;
  }

  char path[40];
  if (basename[0] == '/') {
    snprintf(path, sizeof(path), "%s", basename);
  } else {
    snprintf(path, sizeof(path), "/%s", basename);
  }

  if (!captureToFile(path)) {
    return false;
  }

  if (outName != nullptr && outNameLen > 0) {
    const char *nameOnly = (basename[0] == '/') ? (basename + 1) : basename;
    snprintf(outName, outNameLen, "%s", nameOnly);
  }
  return true;
}

bool senseRenameCapture(const char *fromName, const char *toName) {
  if (fromName == nullptr || toName == nullptr || fromName[0] == '\0' ||
      toName[0] == '\0') {
    setCaptureError("invalid rename args");
    return false;
  }

  if (!mountSdBus(true)) {
    return false;
  }

  char fromPath[40];
  char toPath[40];
  snprintf(fromPath, sizeof(fromPath), "/%s", fromName);
  snprintf(toPath, sizeof(toPath), "/%s", toName);

  if (!SD.exists(fromPath)) {
    shutdownSd();
    setCaptureError("rename source missing");
    return false;
  }
  if (SD.exists(toPath)) {
    SD.remove(toPath);
  }

  const bool ok = SD.rename(fromPath, toPath);
  shutdownSd();
  if (!ok) {
    setCaptureError("SD rename failed");
  }
  return ok;
}

bool senseCaptureImageById(uint16_t imageId) {
  char path[16];
  snprintf(path, sizeof(path), "/%05u.jpg", (unsigned)imageId);
  return captureToFile(path);
}
