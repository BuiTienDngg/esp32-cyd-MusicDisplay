#include "download_manager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <vector>

#include "config.h"
#include "sd_manager.h"
#include "settings.h"
#include "video_player.h"
#include "wifi_manager.h"

namespace {
struct RemoteVideo {
  String id;
  String name;
  String url;
  uint32_t size;
};

std::vector<RemoteVideo> downloadQueue;
HTTPClient transferHttp;
WiFiClient transferClient;
WiFiClientSecure transferTlsClient;
File partialFile;
uint8_t transferBuffer[2048];
size_t queueIndex = 0;
size_t serverCount = 0;
size_t newCount = 0;
uint32_t receivedBytes = 0;
uint32_t lastDataAt = 0;
uint32_t lastLoggedPercent = 0;
uint32_t nextAutoUpdateAt = 0;
bool downloadRequested = false;
bool transferActive = false;
bool finalizePending = false;
bool autoCheckStarted = false;
String activeDownloadId;
String message = "Idle";

bool isSafeVideoId(const String& id) {
  if (id.isEmpty() || id.length() > 48) return false;
  for (size_t index = 0; index < id.length(); ++index) {
    const char value = id[index];
    if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
          (value >= '0' && value <= '9') || value == '_' || value == '-')) {
      return false;
    }
  }
  return true;
}

bool beginHttp(HTTPClient& http, WiFiClient& plainClient,
               WiFiClientSecure& tlsClient, const String& url) {
  http.setConnectTimeout(2500);
  http.setTimeout(1500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (url.startsWith("https://")) {
    tlsClient.setInsecure();
    return http.begin(tlsClient, url);
  }
  return http.begin(plainClient, url);
}

String videoPath(const String& id, const char* extension) {
  return String("/videos/") + id + extension;
}

void closeTransfer() {
  if (partialFile) {
    partialFile.close();
  }
  transferHttp.end();
  transferActive = false;
}

void failTransfer(const char* reason) {
  Serial.printf("[DOWNLOAD] %s: %s\n", activeDownloadId.c_str(), reason);
  closeTransfer();
  SD.remove(videoPath(activeDownloadId, ".tmp"));
  finalizePending = false;
  message = String("Download failed: ") + reason;
  activeDownloadId = "";
}

bool startNextDownload() {
  while (queueIndex < downloadQueue.size()) {
    const RemoteVideo& video = downloadQueue[queueIndex];
    VideoEntry local;
    if (findLocalVideo(video.id, local) && local.size == video.size) {
      ++queueIndex;
      continue;
    }
    if (video.id == currentVideoId()) {
      message = String("Waiting until ") + video.id + " is no longer playing";
      return false;
    }
    ++queueIndex;
    if (sdFreeBytes() < static_cast<uint64_t>(video.size) + 4096) {
      Serial.printf("[DOWNLOAD] Not enough SD space for %s\n", video.id.c_str());
      message = String("Not enough SD space for ") + video.id;
      continue;
    }

    activeDownloadId = video.id;
    const String temporaryPath = videoPath(video.id, ".tmp");
    SD.remove(temporaryPath);
    partialFile = SD.open(temporaryPath, FILE_WRITE);
    if (!partialFile) {
      message = String("Cannot create ") + temporaryPath;
      activeDownloadId = "";
      continue;
    }

    if (!beginHttp(transferHttp, transferClient, transferTlsClient, video.url)) {
      failTransfer("HTTP begin failed");
      continue;
    }
    const int status = transferHttp.GET();
    if (status != HTTP_CODE_OK) {
      failTransfer("HTTP request failed");
      continue;
    }
    const int contentLength = transferHttp.getSize();
    if (contentLength >= 0 && static_cast<uint32_t>(contentLength) != video.size) {
      failTransfer("Content-Length mismatch");
      continue;
    }

    receivedBytes = 0;
    lastDataAt = millis();
    lastLoggedPercent = 0;
    transferActive = true;
    message = String("Downloading ") + video.id;
    Serial.printf("[DOWNLOAD] %s\n", video.id.c_str());
    return true;
  }

  downloadRequested = false;
  activeDownloadId = "";
  message = "Up to date";
  return false;
}

void finishTransfer() {
  if (queueIndex == 0 || receivedBytes != downloadQueue[queueIndex - 1].size ||
      partialFile.size() != downloadQueue[queueIndex - 1].size) {
    failTransfer("Downloaded size mismatch");
    return;
  }

  partialFile.flush();
  partialFile.close();
  transferHttp.end();
  transferActive = false;
  finalizePending = true;
}

void finalizeDownload() {
  if (!finalizePending || activeDownloadId == currentVideoId()) {
    if (finalizePending) {
      message = String("Waiting until ") + activeDownloadId + " is no longer playing";
    }
    return;
  }

  const String id = activeDownloadId;
  const String temporaryPath = videoPath(id, ".tmp");
  const String finalPath = videoPath(id, ".avi");

  if (SD.exists(finalPath)) SD.remove(finalPath);
  if (!SD.rename(temporaryPath, finalPath)) {
    SD.remove(temporaryPath);
    finalizePending = false;
    activeDownloadId = "";
    message = "Could not finalize downloaded file";
    return;
  }

  finalizePending = false;
  scanLocalVideos();
  Serial.printf("[DOWNLOAD] %s Done\n", id.c_str());
  message = String("Downloaded ") + id;
  activeDownloadId = "";
}

void persistManifest(const String& body) {
  SD.mkdir("/cache");
  SD.remove("/cache/manifest.json");
  File file = SD.open("/cache/manifest.json", FILE_WRITE);
  if (file) {
    file.print(body);
    file.close();
  }
}
}

void beginDownloadManager() {
  downloadQueue.clear();
  if (!isSdReady()) return;

  File directory = SD.open("/videos");
  File entry = directory ? directory.openNextFile() : File();
  while (entry) {
    if (!entry.isDirectory()) {
      String path = entry.name();
      if (path.endsWith(".tmp")) {
        entry.close();
        SD.remove(path.startsWith("/") ? path : String("/videos/") + path);
        entry = directory.openNextFile();
        continue;
      }
    }
    entry.close();
    entry = directory.openNextFile();
  }
  if (directory) directory.close();
}

bool checkForNewVideos() {
  if (!wifiConnected()) {
    message = "WiFi is offline";
    return false;
  }
  if (!isSdReady()) {
    message = "SD card is unavailable";
    return false;
  }
  if (transferActive) {
    message = "A download is already running";
    return false;
  }

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure tlsClient;
  if (!beginHttp(http, plainClient, tlsClient, AppConfig::MANIFEST_URL)) {
    message = "Cannot open manifest URL";
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    message = String("Manifest request failed: ") + status;
    Serial.printf("[UPDATE] %s\n", message.c_str());
    return false;
  }

  const int manifestLength = http.getSize();
  if (manifestLength <= 0 || manifestLength > 32768) {
    http.end();
    message = "Manifest length is invalid";
    return false;
  }
  const String body = http.getString();
  http.end();
  if (body.isEmpty() || body.length() > 32768) {
    message = "Manifest is empty or too large";
    return false;
  }

  JsonDocument document;
  if (deserializeJson(document, body) != DeserializationError::Ok ||
      !document["videos"].is<JsonArray>()) {
    message = "Manifest JSON is invalid";
    return false;
  }

  std::vector<RemoteVideo> candidates;
  serverCount = 0;
  for (JsonObjectConst item : document["videos"].as<JsonArrayConst>()) {
    ++serverCount;
    RemoteVideo video;
    video.id = item["id"] | "";
    video.name = item["name"] | video.id;
    video.url = item["url"] | "";
    video.size = item["size"] | 0U;
    const uint8_t fps = item["fps"] | AppConfig::DEFAULT_FPS;
    const uint16_t width = item["width"] | AppConfig::DISPLAY_WIDTH;
    const uint16_t height = item["height"] | AppConfig::DISPLAY_HEIGHT;
    if (!isSafeVideoId(video.id) || video.size == 0 ||
        (!video.url.startsWith("http://") && !video.url.startsWith("https://")) ||
        fps < 10 || fps > 15 || width != AppConfig::DISPLAY_WIDTH ||
        height != AppConfig::DISPLAY_HEIGHT) {
      Serial.printf("[UPDATE] Skipping invalid manifest entry: %s\n", video.id.c_str());
      continue;
    }

    VideoEntry local;
    if (!findLocalVideo(video.id, local) || local.size != video.size) {
      candidates.push_back(video);
    }
  }

  downloadQueue = candidates;
  const String playingId = currentVideoId();
  std::stable_sort(downloadQueue.begin(), downloadQueue.end(),
                   [&playingId](const RemoteVideo& left, const RemoteVideo& right) {
                     return left.id != playingId && right.id == playingId;
                   });
  queueIndex = 0;
  newCount = candidates.size();
  persistManifest(body);
  message = newCount == 0 ? "Up to date" : String(newCount) + " new video(s) available";
  Serial.printf("[UPDATE] Server videos: %u, local videos: %u, new videos: %u\n",
                static_cast<unsigned>(serverCount),
                static_cast<unsigned>(localVideos().size()),
                static_cast<unsigned>(newCount));
  return true;
}

void requestDownloadNewVideos() {
  if (newCount == 0 || downloadQueue.empty()) {
    message = "No new videos to download";
    return;
  }
  queueIndex = 0;
  downloadRequested = true;
  message = "Download queued";
}

void handleDownload() {
  if (finalizePending) {
    finalizeDownload();
    return;
  }
  if (!wifiConnected() || !isSdReady()) {
    if (transferActive) failTransfer("Connection or SD lost");
    return;
  }
  if (!transferActive) {
    if (downloadRequested) startNextDownload();
    return;
  }

  Stream* stream = transferHttp.getStreamPtr();
  if (stream == nullptr) {
    failTransfer("HTTP stream unavailable");
    return;
  }
  const int available = stream->available();
  if (available <= 0) {
    if (receivedBytes == downloadQueue[queueIndex - 1].size) {
      finishTransfer();
    } else if (!transferHttp.connected() || millis() - lastDataAt > 15000) {
      failTransfer("HTTP stream ended early");
    }
    return;
  }

  const uint32_t expected = downloadQueue[queueIndex - 1].size;
  const size_t remaining = expected - receivedBytes;
  const size_t chunkSize = min(static_cast<size_t>(available),
                               min(sizeof(transferBuffer), remaining));
  size_t bytesRead = 0;
  while (bytesRead < chunkSize && stream->available() > 0) {
    const int nextByte = stream->read();
    if (nextByte < 0) break;
    transferBuffer[bytesRead++] = static_cast<uint8_t>(nextByte);
  }
  if (bytesRead == 0) return;
  if (partialFile.write(transferBuffer, bytesRead) != bytesRead) {
    failTransfer("SD write failed");
    return;
  }

  receivedBytes += static_cast<uint32_t>(bytesRead);
  lastDataAt = millis();
  const uint32_t percent = static_cast<uint32_t>(
      (static_cast<uint64_t>(receivedBytes) * 100) / expected);
  if (percent >= lastLoggedPercent + 10 || percent == 100) {
    lastLoggedPercent = percent;
    Serial.printf("[DOWNLOAD] %s %u%%\n", activeDownloadId.c_str(),
                  static_cast<unsigned>(percent));
  }
  if (receivedBytes == expected) finishTransfer();
}

void handleAutoUpdate() {
  AppSettings& settings = applicationSettings();
  if (!settings.autoUpdate || !wifiConnected() || transferActive) return;

  const uint32_t now = millis();
  if (!autoCheckStarted || static_cast<int32_t>(now - nextAutoUpdateAt) >= 0) {
    autoCheckStarted = true;
    nextAutoUpdateAt = now + AppConfig::AUTO_UPDATE_INTERVAL_MS;
    if (checkForNewVideos()) {
      requestDownloadNewVideos();
    } else {
      nextAutoUpdateAt = now + 5UL * 60UL * 1000UL;
    }
  }
}

size_t serverVideoCount() { return serverCount; }
size_t newVideoCount() { return newCount; }
size_t pendingDownloadCount() {
  return downloadRequested && queueIndex < downloadQueue.size()
             ? downloadQueue.size() - queueIndex + ((transferActive || finalizePending) ? 1 : 0)
             : ((transferActive || finalizePending) ? 1 : 0);
}
bool downloadInProgress() { return transferActive || finalizePending; }
uint8_t downloadProgressPercent() {
  if (finalizePending) return 100;
  if (!transferActive || downloadQueue.empty() || queueIndex == 0) return 0;
  const uint32_t expected = downloadQueue[queueIndex - 1].size;
  return expected == 0 ? 0 : static_cast<uint8_t>(
      min<uint32_t>(100, (static_cast<uint64_t>(receivedBytes) * 100) / expected));
}
String downloadingVideoId() { return activeDownloadId; }
String updateMessage() { return message; }