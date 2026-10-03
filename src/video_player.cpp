#include "video_player.h"

#include <JPEGDEC.h>
#include <SD.h>

#include "display.h"
#include "sd_manager.h"
#include "settings.h"

namespace {
constexpr size_t MAX_FRAME_BYTES = AppConfig::MAX_VIDEO_FRAME_BYTES;
uint8_t* frameBuffer = nullptr;
JPEGDEC jpeg;
File videoFile;
AppSettings& settings = applicationSettings();
String activeId;
String activeName;
uint32_t moviStart = 0;
uint32_t moviEnd = 0;
uint32_t frameIntervalMs = 1000 / AppConfig::DEFAULT_FPS;
uint32_t nextFrameAt = 0;
bool playerReady = false;
bool playing = false;

bool ensureFrameBuffer() {
  if (frameBuffer != nullptr) {
    return true;
  }
  frameBuffer = static_cast<uint8_t*>(malloc(MAX_FRAME_BYTES));
  if (frameBuffer == nullptr) {
    Serial.println("[PLAYER] Not enough heap for JPEG frame buffer");
    showDisplayMessage("MEMORY ERROR", "Cannot allocate frame buffer");
    return false;
  }
  return true;
}

uint32_t readUint32(File& file) {
  uint8_t bytes[4];
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
    return 0;
  }
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

int drawJpegBlock(JPEGDRAW* block) {
  drawDisplayImage(block->x, block->y, block->iWidth, block->iHeight,
                   block->pPixels);
  return 1;
}

void readAviHeaderList(uint32_t listEnd) {
  while (videoFile.position() + 8 <= listEnd) {
    const uint32_t chunkStart = videoFile.position();
    char chunkId[5] = {};
    if (videoFile.readBytes(chunkId, 4) != 4) return;
    const uint32_t chunkSize = readUint32(videoFile);
    const uint32_t chunkEnd = chunkStart + 8 + chunkSize + (chunkSize & 1U);
    if (chunkEnd > listEnd) return;
    if (strncmp(chunkId, "avih", 4) == 0 && chunkSize >= 4) {
      const uint32_t microsecondsPerFrame = readUint32(videoFile);
      if (microsecondsPerFrame >= 10000 && microsecondsPerFrame <= 1000000) {
        frameIntervalMs = max<uint32_t>(1, microsecondsPerFrame / 1000);
      }
    }
    if (!videoFile.seek(chunkEnd)) return;
  }
}

bool findMoviList() {
  if (!videoFile || !videoFile.seek(12)) {
    Serial.println("[PLAYER] Cannot seek to AVI chunks");
    return false;
  }

  while (videoFile.position() + 8 <= videoFile.size()) {
    const uint32_t chunkStart = videoFile.position();
    char chunkId[5] = {};
    if (videoFile.readBytes(chunkId, 4) != 4) {
      Serial.println("[PLAYER] Failed reading AVI chunk ID");
      return false;
    }
    const uint32_t chunkSize = readUint32(videoFile);
    if (chunkSize > videoFile.size() - videoFile.position()) {
      Serial.printf("[PLAYER] AVI chunk %.4s exceeds file bounds (%u bytes)\n",
                    chunkId, static_cast<unsigned>(chunkSize));
      return false;
    }

    if (strncmp(chunkId, "avih", 4) == 0 && chunkSize >= 4) {
      const uint32_t microsecondsPerFrame = readUint32(videoFile);
      if (microsecondsPerFrame >= 10000 && microsecondsPerFrame <= 1000000) {
        frameIntervalMs = max<uint32_t>(1, microsecondsPerFrame / 1000);
      }
    } else if (strncmp(chunkId, "LIST", 4) == 0 && chunkSize >= 4) {
      char listType[5] = {};
      if (videoFile.readBytes(listType, 4) != 4) {
        Serial.println("[PLAYER] Failed reading AVI LIST type");
        return false;
      }
      if (strncmp(listType, "movi", 4) == 0) {
        moviStart = videoFile.position();
        moviEnd = chunkStart + 8 + chunkSize;
        Serial.printf("[PLAYER] AVI movi found at %u..%u\n",
                      static_cast<unsigned>(moviStart),
                      static_cast<unsigned>(moviEnd));
        return true;
      } else if (strncmp(listType, "hdrl", 4) == 0) {
        readAviHeaderList(chunkStart + 8 + chunkSize);
      }
    }

    const uint32_t nextChunk = chunkStart + 8 + chunkSize + (chunkSize & 1U);
    if (nextChunk > videoFile.size() || !videoFile.seek(nextChunk)) {
      Serial.printf("[PLAYER] Invalid next AVI chunk offset %u (file %u)\n",
                    static_cast<unsigned>(nextChunk),
                    static_cast<unsigned>(videoFile.size()));
      return false;
    }
  }
  Serial.println("[PLAYER] AVI has no movi list");
  return false;
}

bool openVideo(const VideoEntry& video) {
  if (videoFile) {
    videoFile.close();
  }
  videoFile = SD.open(video.path, FILE_READ);
  frameIntervalMs = 1000 / AppConfig::DEFAULT_FPS;
  if (!videoFile || videoFile.size() < 16) {
    Serial.printf("[PLAYER] SD.open failed for '%s'\n", video.path.c_str());
    return false;
  }

  char signature[4];
  if (videoFile.readBytes(signature, sizeof(signature)) != sizeof(signature) ||
      strncmp(signature, "RIFF", 4) != 0) {
    Serial.println("[PLAYER] Invalid RIFF signature");
    videoFile.close();
    return false;
  }
  const uint32_t riffSize = readUint32(videoFile);
  if (riffSize < 4 || riffSize > videoFile.size() - 8 ||
      videoFile.readBytes(signature, sizeof(signature)) != sizeof(signature) ||
      strncmp(signature, "AVI ", 4) != 0) {
    Serial.println("[PLAYER] Invalid AVI form type");
    videoFile.close();
    return false;
  }
  if (!findMoviList()) {
    videoFile.close();
    return false;
  }

  activeId = video.id;
  activeName = video.name;
  videoFile.seek(moviStart);
  nextFrameAt = 0;
  settings.currentVideoId = activeId;
  saveSettings(settings);
  return true;
}

bool drawNextFrame() {
  while (videoFile && videoFile.position() + 8 <= moviEnd) {
    char chunkId[5] = {};
    if (videoFile.readBytes(chunkId, 4) != 4) {
      return false;
    }
    const uint32_t chunkSize = readUint32(videoFile);
    const uint32_t chunkEnd = videoFile.position() + chunkSize + (chunkSize & 1U);
    if (chunkEnd > moviEnd || chunkEnd > videoFile.size()) {
      return false;
    }

    if (strncmp(chunkId, "00dc", 4) == 0 || strncmp(chunkId, "00db", 4) == 0) {
      if (chunkSize > MAX_FRAME_BYTES) {
        videoFile.seek(chunkEnd);
        continue;
      }
      const size_t bytesRead = videoFile.read(frameBuffer, chunkSize);
      videoFile.seek(chunkEnd);
      if (bytesRead != chunkSize || !jpeg.openRAM(frameBuffer, chunkSize, drawJpegBlock)) {
        return false;
      }
      jpeg.setPixelType(RGB565_BIG_ENDIAN);
      jpeg.decode(0, 0, JPEG_SCALE_HALF);
      jpeg.close();
      nextFrameAt = millis() + frameIntervalMs;
      return true;
    }

    videoFile.seek(chunkEnd);
  }
  return false;
}

int currentIndex() {
  const auto& videos = localVideos();
  for (size_t index = 0; index < videos.size(); ++index) {
    if (videos[index].id == activeId) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool startIndex(size_t index) {
  const auto& videos = localVideos();
  if (index >= videos.size() || !openVideo(videos[index])) {
    Serial.printf("[PLAYER] Cannot open AVI: %s\n", index < videos.size() ? videos[index].id.c_str() : "invalid index");
    playing = false;
    return false;
  }
  playing = true;
  settings.mode = settings.mode == PLAY_PAUSE ? PLAY_RANDOM : settings.mode;
  return true;
}

void handleEndOfVideo() {
  const int index = currentIndex();
  const size_t count = localVideos().size();
  if (settings.mode == PLAY_LOOP_ONE) {
    videoFile.seek(moviStart);
    nextFrameAt = 0;
    return;
  }
  if (count == 0 || index < 0) {
    playing = false;
    return;
  }
  if (settings.mode == PLAY_RANDOM && count > 1) {
    size_t next = static_cast<size_t>(index);
    while (next == static_cast<size_t>(index)) {
      next = static_cast<size_t>(random(count));
    }
    startIndex(next);
  } else if (settings.mode == PLAY_LOOP_ALL ||
             (settings.mode == PLAY_RANDOM && count == 1)) {
    startIndex((static_cast<size_t>(index) + 1) % count);
  } else if (static_cast<size_t>(index + 1) < count) {
    startIndex(static_cast<size_t>(index + 1));
  } else {
    playing = false;
    videoFile.close();
  }
}
}

void beginVideoPlayer() {
  loadSettings(settings);
  if (settings.mode == PLAY_PAUSE) {
    settings.mode = PLAY_RANDOM;
  }

  scanLocalVideos();
  if (localVideos().empty()) {
    showDisplayMessage("NO VIDEOS", "Put MJPEG AVI files in /videos");
    Serial.println("[PLAYER] No .avi clips found in /videos");
    return;
  }
  if (!ensureFrameBuffer()) {
    return;
  }

  size_t selected = 0;
  VideoEntry previous;
  bool foundPrevious = !settings.currentVideoId.isEmpty() &&
                       findLocalVideo(settings.currentVideoId, previous);
  if (foundPrevious) {
    for (size_t index = 0; index < localVideos().size(); ++index) {
      if (localVideos()[index].id == previous.id) {
        selected = index;
        break;
      }
    }
  } else if (settings.mode == PLAY_RANDOM) {
    selected = static_cast<size_t>(random(localVideos().size()));
  }

  const String selectedId = localVideos()[selected].id;
  if (settings.shuffle) {
    shufflePlaylist();
    for (size_t index = 0; index < localVideos().size(); ++index) {
      if (localVideos()[index].id == selectedId) {
        selected = index;
        break;
      }
    }
  }

  playerReady = true;
  if (!startIndex(selected)) {
    showDisplayMessage("VIDEO ERROR", "Check AVI format and SD card");
  }
}

void handleVideo() {
  if (!playerReady && !localVideos().empty()) {
    if (!ensureFrameBuffer()) {
      return;
    }
    playerReady = true;
    const size_t selected = settings.mode == PLAY_RANDOM
                                ? static_cast<size_t>(random(localVideos().size()))
                                : 0;
    if (!startIndex(selected)) {
      showDisplayMessage("VIDEO ERROR", "Check AVI format and SD card");
    }
  }
  if (!playerReady || settings.mode == PLAY_PAUSE || !playing) {
    return;
  }
  if (nextFrameAt != 0 && static_cast<int32_t>(millis() - nextFrameAt) < 0) {
    return;
  }
  if (!drawNextFrame()) {
    handleEndOfVideo();
  }
}

void setPlaybackMode(PlaybackMode mode) {
  const PlaybackMode previousMode = settings.mode;
  settings.mode = mode;
  if (mode == PLAY_PAUSE) {
    playing = false;
  } else if (previousMode == PLAY_PAUSE && videoFile) {
    playing = true;
    nextFrameAt = 0;
  } else if (!playing && !activeId.isEmpty()) {
    VideoEntry current;
    if (findLocalVideo(activeId, current)) {
      startIndex(static_cast<size_t>(currentIndex()));
    }
  }
  saveSettings(settings);
}

void playVideo() {
  if (activeId.isEmpty()) {
    if (!localVideos().empty()) {
      startIndex(0);
    }
    return;
  }
  if (settings.mode == PLAY_PAUSE) {
    settings.mode = PLAY_RANDOM;
  }
  if (!videoFile) {
    VideoEntry current;
    if (findLocalVideo(activeId, current)) {
      startIndex(static_cast<size_t>(currentIndex()));
    }
  } else {
    playing = true;
  }
  saveSettings(settings);
}

bool playVideo(const String& id) {
  return selectVideo(id);
}

void playNext() {
  const size_t count = localVideos().size();
  if (count == 0) return;
  const int index = currentIndex();
  startIndex(index < 0 ? 0 : (static_cast<size_t>(index + 1) % count));
}

void playPrevious() {
  const size_t count = localVideos().size();
  if (count == 0) return;
  const int index = currentIndex();
  startIndex(index <= 0 ? count - 1 : static_cast<size_t>(index - 1));
}

bool selectVideo(const String& id) {
  VideoEntry video;
  if (!findLocalVideo(id, video)) return false;
  for (size_t index = 0; index < localVideos().size(); ++index) {
    if (localVideos()[index].id == id) {
      return startIndex(index);
    }
  }
  return false;
}

void loopCurrentVideo() {
  if (!activeId.isEmpty()) {
    setPlaybackMode(PLAY_LOOP_ONE);
  }
}

void shufflePlaylist() {
  shuffleLocalVideos();
}

String currentVideoId() { return activeId; }
String currentVideoName() { return activeName; }
bool isVideoPlaying() { return playing; }
PlaybackMode currentPlaybackMode() { return settings.mode; }