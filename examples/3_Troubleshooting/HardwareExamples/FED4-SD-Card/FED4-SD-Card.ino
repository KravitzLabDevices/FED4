/*
 * FED4 SD Card Test
 *
 * Uses SPI/CS mapping from FED4_Pins.h:
 *   SCK=12, MISO=11, MOSI=13, SD_CS=48
 */

#include <FS.h>
#include <SD.h>
#include <SPI.h>

#define SPI_SCK 12
#define SPI_MISO 11
#define SPI_MOSI 13
#define SD_CS 48
#define DISPLAY_CS 44

void listDir(fs::FS &fs, const char *dirname) {
  Serial.printf("Listing directory: %s\n", dirname);
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.print(file.isDirectory() ? "  DIR : " : "  FILE: ");
    Serial.print(file.name());
    if (!file.isDirectory()) {
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    } else {
      Serial.println();
    }
    file = root.openNextFile();
  }
}

void writeReadDemo() {
  const char *path = "/fed4_sd_test.txt";
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("Open for write failed");
    return;
  }
  f.println("FED4 SD test");
  f.close();

  f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.println("Open for read failed");
    return;
  }
  Serial.print("Read: ");
  while (f.available()) Serial.write(f.read());
  f.close();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Keep display deselected when sharing SPI bus.
  pinMode(DISPLAY_CS, OUTPUT);
  digitalWrite(DISPLAY_CS, LOW);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  if (!SD.begin(SD_CS, SPI, 4000000)) {
    Serial.println("SD mount failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  Serial.printf("Total: %lluMB  Used: %lluMB\n",
                SD.totalBytes() / (1024ULL * 1024ULL),
                SD.usedBytes() / (1024ULL * 1024ULL));

  listDir(SD, "/");
  writeReadDemo();
}

void loop() {}
