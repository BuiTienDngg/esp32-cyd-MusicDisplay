#include <Arduino.h>
#include <esp_system.h>

#include "display.h"
#include "download_manager.h"
#include "config.h"
#include "sd_manager.h"
#include "video_player.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace {
String emptyScreenMessage;
bool webServerStarted = false;

void updateEmptyScreen() {
  if (!localVideos().empty()) {
    emptyScreenMessage = "";
    return;
  }

    const String nextMessage = !isSdReady()
               ? "SD card unavailable"
               : (downloadInProgress()
                 ? "Downloading first video..."
                 : (WIFI_SSID[0] == '\0'
                   ? "Offline: add AVI to /videos"
                   : (wifiConnected()
                     ? "Checking for new videos..."
                     : "Connecting WiFi...")));
  if (nextMessage != emptyScreenMessage) {
    emptyScreenMessage = nextMessage;
      showDisplayMessage(isSdReady() ? "NO VIDEOS" : "SD ERROR",
          emptyScreenMessage.c_str());
  }
}
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  Serial.printf("[BOOT] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
  initDisplay();
  Serial.println("[BOOT] CYD ambient display starting");
  showDisplayMessage("AMBIENT DISPLAY", "Starting SD card...");
  const bool sdReady = initSdCard();
  beginDownloadManager();
  if (!sdReady) {
    showDisplayMessage("SD ERROR", "Check card and wiring");
  } else {
    beginVideoPlayer();
  }
  beginWifi();
}

void loop() {
  handleVideo();
  handleWifi();
  if (wifiConnected()) {
    if (!webServerStarted) {
      beginWebServer();
      webServerStarted = true;
    }
    handleWebServer();
  }
  handleAutoUpdate();
  handleDownload();
  updateEmptyScreen();
}