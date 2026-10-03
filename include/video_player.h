#pragma once

#include "config.h"

void beginVideoPlayer();
void handleVideo();
void setPlaybackMode(PlaybackMode mode);
void playVideo();
bool playVideo(const String& id);
void playNext();
void playPrevious();
bool selectVideo(const String& id);
void loopCurrentVideo();
void shufflePlaylist();
String currentVideoId();
String currentVideoName();
bool isVideoPlaying();
PlaybackMode currentPlaybackMode();