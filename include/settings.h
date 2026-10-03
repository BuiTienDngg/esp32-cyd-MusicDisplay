#pragma once

#include "config.h"

struct AppSettings {
  PlaybackMode mode = PLAY_RANDOM;
  String currentVideoId;
  uint8_t brightness = 100;
  bool autoUpdate = true;
  bool shuffle = true;
};

void loadSettings(AppSettings& settings);
bool saveSettings(const AppSettings& settings);
AppSettings& applicationSettings();
const char* playbackModeName(PlaybackMode mode);
bool parsePlaybackMode(const String& value, PlaybackMode& mode);