#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef AMBIENT_MANIFEST_URL
#define AMBIENT_MANIFEST_URL "http://192.168.55.101:8000/manifest.json"
#endif

enum PlaybackMode : uint8_t {
  PLAY_CONTINUOUS,
  PLAY_RANDOM,
  PLAY_LOOP_ONE,
  PLAY_LOOP_ALL,
  PLAY_PAUSE
};

namespace AppConfig {
constexpr uint8_t TFT_BACKLIGHT_PIN = 21;
constexpr uint8_t SD_CS_PIN = 5;
constexpr int8_t SD_SCK_PIN = 18;
constexpr int8_t SD_MISO_PIN = 19;
constexpr int8_t SD_MOSI_PIN = 23;
constexpr uint16_t DISPLAY_WIDTH = 320;
constexpr uint16_t DISPLAY_HEIGHT = 240;
constexpr uint8_t DEFAULT_FPS = 15;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t AUTO_UPDATE_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr size_t MAX_VIDEO_FRAME_BYTES = 64 * 1024;
constexpr char MANIFEST_URL[] = AMBIENT_MANIFEST_URL;
}