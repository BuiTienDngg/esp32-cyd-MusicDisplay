#include "sd_manager.h"

#include <SD.h>
#include <SPI.h>
#include <utility>

#include "config.h"

namespace {
SPIClass sdSpi(VSPI);
std::vector<VideoEntry> videos;
bool sdReady = false;

String videoIdFromName(const String& name) {
  int slash = name.lastIndexOf('/');
  String baseName = slash >= 0 ? name.substring(slash + 1) : name;
  int dot = baseName.lastIndexOf('.');
  if (dot >= 0) {
    baseName = baseName.substring(0, dot);
  }
  return baseName;
}
}

bool initSdCard() {
  sdSpi.begin(AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN,
              AppConfig::SD_MOSI_PIN, AppConfig::SD_CS_PIN);
  sdReady = SD.begin(AppConfig::SD_CS_PIN, sdSpi, 20000000);
  if (!sdReady) {
    Serial.printf("[SD] Card mount failed (CS=%u, SCK=%d, MISO=%d, MOSI=%d)\n",
                  AppConfig::SD_CS_PIN, AppConfig::SD_SCK_PIN,
                  AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN);
    return false;
  }

  SD.mkdir("/videos");
  SD.mkdir("/config");
  SD.mkdir("/cache");
  scanLocalVideos();
  Serial.printf("[SD] Ready, type=%u, size=%llu bytes, %u local video(s)\n",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize()),
                static_cast<unsigned>(videos.size()));
  return true;
}

bool isSdReady() {
  return sdReady;
}

void scanLocalVideos() {
  videos.clear();
  if (!sdReady) {
    return;
  }

  File directory = SD.open("/videos");
  if (!directory || !directory.isDirectory()) {
    return;
  }

  File entry = directory.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String path = entry.name();
      String lowerPath = path;
      lowerPath.toLowerCase();
      if (lowerPath.endsWith(".avi")) {
        VideoEntry video;
        video.id = videoIdFromName(path);
        video.name = video.id;
        video.name.replace('_', ' ');
        video.path = path.startsWith("/") ? path : String("/videos/") + path;
        video.size = entry.size();
        videos.push_back(video);
      }
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();
}

const std::vector<VideoEntry>& localVideos() {
  return videos;
}

bool findLocalVideo(const String& id, VideoEntry& result) {
  for (const VideoEntry& video : videos) {
    if (video.id == id) {
      result = video;
      return true;
    }
  }
  return false;
}

bool removeLocalVideo(const String& id) {
  VideoEntry video;
  if (!sdReady || !findLocalVideo(id, video) || !SD.remove(video.path)) {
    return false;
  }
  scanLocalVideos();
  return true;
}

uint64_t sdFreeBytes() {
  if (!sdReady) {
    return 0;
  }
  const uint64_t total = SD.totalBytes();
  const uint64_t used = SD.usedBytes();
  return total > used ? total - used : 0;
}

void shuffleLocalVideos() {
  for (size_t index = videos.size(); index > 1; --index) {
    std::swap(videos[index - 1], videos[static_cast<size_t>(random(index))]);
  }
}