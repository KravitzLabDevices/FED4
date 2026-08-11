/*
 * Pin map for Seeed Studio XIAO ESP32-S3 Sense (OV3660).
 * From ESP32 core CameraWebServer example.
 */

#pragma once

#define CAMERA_MODEL_XIAO_ESP32S3

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39

#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

// Sense expansion board microSD (SPI) — see Seeed pin-multiplexing wiki
#define SD_CS_PIN   21
#define SD_SCK_PIN  7
#define SD_MISO_PIN 8
#define SD_MOSI_PIN 9
#define SD_SPI_FREQ 4000000
