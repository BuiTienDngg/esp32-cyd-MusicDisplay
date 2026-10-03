#pragma once
#include <Arduino.h>
#include "apple_media_service.h"
class LEDManager {
public:
    void begin();
    void updateLEDs(AppleMediaService::MediaInformation::PlaybackState playbackStatus, bool isConnected);
    void disable();
};
