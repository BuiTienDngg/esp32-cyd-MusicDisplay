#pragma once
#include <TFT_eSPI.h>
#include "cyd_panel.h"
#include "apple_media_service.h"
class DisplayManager {
public:
    DisplayManager();
    void begin();
    void updateDisplay(const AppleMediaService::MediaInformation& media, bool connected, int highlighted = -1);
    void disable();
private:
    TFT_eSPI tft;
    TFT_eSprite sprite;
    CydPanel panel;
    bool enabled = false;
    bool bufferReady = false;
    uint32_t lastElapsedTick = 0;
    float lastReportedElapsed = -1;
    float elapsedTime = 0;
    String lastTitle;
    String scrollingText[3];
    String renderedText[3];
    int scrollX[3] = {12, 12, 12};
    uint32_t scrollTick[3] = {}, scrollHoldUntil[3] = {};
    void drawText(const String& text, int y, uint16_t color, int row);
    void drawControls(bool playing, bool connected, int highlighted);
};
