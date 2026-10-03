#pragma once

#include <Arduino.h>

void beginDownloadManager();
bool checkForNewVideos();
void requestDownloadNewVideos();
void handleDownload();
void handleAutoUpdate();
size_t serverVideoCount();
size_t newVideoCount();
size_t pendingDownloadCount();
bool downloadInProgress();
uint8_t downloadProgressPercent();
String downloadingVideoId();
String updateMessage();