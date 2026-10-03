#include "settings.h"

#include <ArduinoJson.h>
#include <SD.h>

#include "sd_manager.h"

namespace {
constexpr char SETTINGS_PATH[] = "/config/settings.json";
constexpr char SETTINGS_TEMP_PATH[] = "/config/settings.tmp";
constexpr char SETTINGS_BACKUP_PATH[] = "/config/settings.bak";
AppSettings storedSettings;
}

AppSettings& applicationSettings() {
  return storedSettings;
}

const char* playbackModeName(PlaybackMode mode) {
  switch (mode) {
    case PLAY_CONTINUOUS: return "continuous";
    case PLAY_RANDOM: return "random";
    case PLAY_LOOP_ONE: return "loop_one";
    case PLAY_LOOP_ALL: return "loop_all";
    case PLAY_PAUSE: return "pause";
  }
  return "random";
}

bool parsePlaybackMode(const String& value, PlaybackMode& mode) {
  if (value == "continuous") mode = PLAY_CONTINUOUS;
  else if (value == "random") mode = PLAY_RANDOM;
  else if (value == "loop_one") mode = PLAY_LOOP_ONE;
  else if (value == "loop_all") mode = PLAY_LOOP_ALL;
  else if (value == "pause") mode = PLAY_PAUSE;
  else return false;
  return true;
}

void loadSettings(AppSettings& settings) {
  settings = AppSettings{};
  if (!isSdReady()) {
    return;
  }
  const char* settingsPath = SD.exists(SETTINGS_PATH)
                                 ? SETTINGS_PATH
                                 : SETTINGS_BACKUP_PATH;
  if (!SD.exists(settingsPath)) {
    saveSettings(settings);
    return;
  }

  File file = SD.open(settingsPath, FILE_READ);
  if (!file) {
    return;
  }

  JsonDocument document;
  if (deserializeJson(document, file) == DeserializationError::Ok) {
    PlaybackMode mode;
    if (parsePlaybackMode(document["mode"] | "random", mode)) {
      settings.mode = mode;
    }
    settings.currentVideoId = document["current_video"] | "";
    settings.brightness = document["brightness"] | 100;
    settings.autoUpdate = document["auto_update"] | true;
    settings.shuffle = document["shuffle"] | true;
  }
  file.close();
}

bool saveSettings(const AppSettings& settings) {
  if (!isSdReady()) {
    return false;
  }
  if (!SD.exists("/config")) {
    SD.mkdir("/config");
  }
  SD.remove(SETTINGS_TEMP_PATH);
  File file = SD.open(SETTINGS_TEMP_PATH, FILE_WRITE);
  if (!file) {
    return false;
  }

  JsonDocument document;
  document["mode"] = playbackModeName(settings.mode);
  document["current_video"] = settings.currentVideoId;
  document["brightness"] = settings.brightness;
  document["auto_update"] = settings.autoUpdate;
  document["shuffle"] = settings.shuffle;
  const bool success = serializeJson(document, file) > 0;
  file.flush();
  file.close();
  if (!success) {
    SD.remove(SETTINGS_TEMP_PATH);
    return false;
  }

  SD.remove(SETTINGS_BACKUP_PATH);
  if (SD.exists(SETTINGS_PATH) && !SD.rename(SETTINGS_PATH, SETTINGS_BACKUP_PATH)) {
    SD.remove(SETTINGS_TEMP_PATH);
    return false;
  }
  if (!SD.rename(SETTINGS_TEMP_PATH, SETTINGS_PATH)) {
    if (SD.exists(SETTINGS_BACKUP_PATH)) {
      SD.rename(SETTINGS_BACKUP_PATH, SETTINGS_PATH);
    }
    return false;
  }
  SD.remove(SETTINGS_BACKUP_PATH);
  return true;
}