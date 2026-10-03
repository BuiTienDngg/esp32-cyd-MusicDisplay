#pragma once

#include <Arduino.h>
#include <vector>

struct VideoEntry {
  String id;
  String name;
  String path;
  uint32_t size;
};

bool initSdCard();
bool isSdReady();
void scanLocalVideos();
const std::vector<VideoEntry>& localVideos();
bool findLocalVideo(const String& id, VideoEntry& result);
bool removeLocalVideo(const String& id);
uint64_t sdFreeBytes();
void shuffleLocalVideos();