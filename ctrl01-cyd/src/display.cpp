#include "display.h"
#include "cyd_board.h"
#include "logger.h"
#include "fonts/VietnameseSans28.h"
#include "fonts/VietnameseSans20.h"
#include "vietnamese_utf8.h"
#include <cmath>

namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t Background = rgb(8,12,24), Card = rgb(24,28,64);
constexpr uint16_t Surface = rgb(32,40,64), Muted = rgb(144,160,192);
constexpr uint16_t Cyan = rgb(48,224,240), Pink = rgb(232,80,192);
constexpr uint16_t Green = rgb(96,240,160), Track = rgb(48,56,80);
const uint16_t Accent[] = {Cyan, Pink, Cyan, rgb(176,144,248), rgb(176,144,248)};

String timeLabel(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0) return "--:--";
    const unsigned total = unsigned(constrain(seconds, 0.0f, 86400.0f));
    char label[16];
    snprintf(label, sizeof(label), "%u:%02u", total / 60, total % 60);
    return label;
}
}

DisplayManager::DisplayManager() : tft(240,320), sprite(&tft) {}

void DisplayManager::begin() {
    tft.init();
    tft.setRotation(CYD::Rotation);
    if (tft.width() != CYD::Width || tft.height() != CYD::Height) {
        LOG_ERROR("TFT rotation/dimensions mismatch: %dx%d", tft.width(), tft.height());
        return;
    }
    tft.fillScreen(TFT_BLACK);
    ledcSetup(0,5000,8); ledcAttachPin(TFT_BL,0); ledcWrite(0,127);
    panel.begin();
    sprite.setColorDepth(8); // 76,800 bytes, leaving room for BLE without PSRAM.
    sprite.setTextWrap(false,false);
    bufferReady = sprite.createSprite(CYD::Width,CYD::Height) != nullptr;
    if (!bufferReady) LOG_ERROR("Cannot allocate CYD display sprite");
    enabled = true;
}

void DisplayManager::disable() {
    ledcWrite(0,0); panel.sleep(); enabled = false;
}

void DisplayManager::drawText(const String& text, int y, uint16_t color, int row) {
    if (row == 0) sprite.loadFont(VietnameseSans28);
    else if (row == 1) sprite.loadFont(VietnameseSans20);
    sprite.setTextSize(1); sprite.setTextColor(color,Card);
    const uint32_t now = millis();
    if (scrollingText[row] != text) {
        scrollingText[row] = text;
        renderedText[row] = normalizeVietnameseUtf8(text);
        scrollX[row] = 12; scrollTick[row] = now; scrollHoldUntil[row] = now + 1200;
    }
    const String& rendered = renderedText[row];
    const int width = sprite.textWidth(rendered);
    if (width <= CYD::Width - 24) sprite.drawString(rendered,12,y);
    else {
        sprite.drawString(rendered,scrollX[row],y);
        if (int32_t(now - scrollHoldUntil[row]) < 0) scrollTick[row] = now;
        else {
            const unsigned steps = min(12U, unsigned((now - scrollTick[row]) * 28 / 1000));
            if (steps) { scrollX[row] -= steps; scrollTick[row] = now; }
            if (scrollX[row] < -width) { scrollX[row] = CYD::Width-12; scrollHoldUntil[row] = now+300; }
        }
    }
    // Clip scrolling text to the music card without changing its glyph metrics.
    const int height = sprite.fontHeight();
    sprite.fillRect(0,y,8,height,Background);
    sprite.fillRect(8,y,4,height,Card);
    sprite.fillRect(CYD::Width-12,y,4,height,Card);
    sprite.fillRect(CYD::Width-8,y,8,height,Background);
}

void DisplayManager::drawControls(bool playing, bool connected, int highlighted) {
    sprite.unloadFont(); sprite.setTextFont(1); sprite.setTextSize(1);
    const char* labels[] = {"PREV",playing ? "PAUSE" : "PLAY","NEXT","VOL-","VOL+"};
    for (int i = 0; i < 5; ++i) {
        const int left = i*CYD::ControlWidth+4, width = CYD::ControlWidth-8;
        const int center = i*CYD::ControlWidth+CYD::ControlWidth/2;
        const bool pressed = i == highlighted;
        const uint16_t face = pressed ? Accent[i] : (connected && i==1 ? rgb(160,48,144) : Surface);
        const uint16_t ink = pressed ? Background : (connected ? TFT_WHITE : Muted);
        sprite.fillRoundRect(left,CYD::ControlsY+2,width,CYD::ControlHeight,8,Background);
        sprite.fillRoundRect(left,CYD::ControlsY,width,CYD::ControlHeight-2,8,face);
        sprite.drawRoundRect(left,CYD::ControlsY,width,CYD::ControlHeight-2,8,pressed ? TFT_WHITE : (connected ? Accent[i] : Track));
        const int cy = CYD::ControlsY+13;
        if (i==0 || i==2) {
            const int direction = i==0 ? -1 : 1;
            sprite.fillTriangle(center+direction*6,cy,center-direction*2,cy-6,center-direction*2,cy+6,ink);
            sprite.fillRect(center+direction*8-1,cy-6,2,13,ink);
        } else if (i==1) {
            if (playing) {
                sprite.fillRect(center-5,cy-6,3,12,ink); sprite.fillRect(center+2,cy-6,3,12,ink);
            } else sprite.fillTriangle(center-4,cy-6,center-4,cy+6,center+6,cy,ink);
        } else {
            sprite.fillRect(center-6,cy-1,12,2,ink);
            if (i==4) sprite.fillRect(center-1,cy-6,2,12,ink);
        }
        sprite.setTextDatum(MC_DATUM); sprite.setTextColor(ink,face);
        sprite.drawString(labels[i],center,CYD::ControlsY+31);
    }
    sprite.setTextDatum(TL_DATUM);
}

void DisplayManager::updateDisplay(const AppleMediaService::MediaInformation& media, bool connected, int highlighted) {
    if (!bufferReady) return;
    const bool waking = !enabled;
    if (waking) { panel.wake(); enabled = true; }
    sprite.unloadFont(); sprite.setTextFont(1); sprite.setTextSize(1);
    sprite.setTextDatum(TL_DATUM); sprite.fillSprite(Background);
    const bool playing = media.mPlaybackState == AppleMediaService::MediaInformation::PlaybackState::Playing;
    const uint32_t now = millis();
    if (lastTitle != media.mTitle.c_str() || lastReportedElapsed != media.mElapsedTime) {
        elapsedTime = media.mElapsedTime; lastReportedElapsed = media.mElapsedTime;
        lastTitle = media.mTitle.c_str(); lastElapsedTick = now;
    } else if (connected && playing) elapsedTime += (now-lastElapsedTick)/1000.0f;
    lastElapsedTick = now;
    const float volume = std::isfinite(media.mVolume) ? constrain(media.mVolume,0.0f,1.0f) : 0;

    // Compact header and actual iPhone volume, with cyan/magenta accents.
    sprite.fillRect(0,0,CYD::Width/2,3,Cyan);
    sprite.fillRect(CYD::Width/2,0,CYD::Width-CYD::Width/2,3,Pink);
    sprite.setTextColor(TFT_WHITE,Background); sprite.drawString("CTRL 01",12,10,2);
    sprite.setTextColor(Muted,Background); sprite.drawString("MUSIC REMOTE",12,28,1);
    const int volumeX = CYD::Width-144;
    sprite.setTextColor(Cyan,Background);
    sprite.drawString("VOL " + String(int(volume*100)) + "%",volumeX,13,1);
    sprite.fillRoundRect(volumeX,27,58,4,2,Track);
    if (volume>0) sprite.fillRect(volumeX,27,int(volume*58),4,Cyan);
    const uint16_t badge = connected ? rgb(24,72,48) : rgb(80,32,64);
    sprite.fillRoundRect(CYD::Width-72,10,62,24,12,badge);
    sprite.fillCircle(CYD::Width-62,22,3,connected ? Green : Pink);
    sprite.setTextDatum(MC_DATUM); sprite.setTextColor(connected ? Green : Pink,badge);
    sprite.drawString(connected ? "READY" : "PAIR",CYD::Width-38,22,1);
    sprite.setTextDatum(TL_DATUM);

    sprite.fillRoundRect(8,43,CYD::Width-16,122,12,rgb(8,16,40));
    sprite.fillRoundRect(8,40,CYD::Width-16,122,12,Card);
    sprite.setTextColor(connected ? Cyan : Pink,Card);
    sprite.drawString(connected ? (playing ? "NOW PLAYING" : "PAUSED") : "PAIR WITH IPHONE",16,47,1);
    sprite.fillCircle(CYD::Width-24,51,3,connected && playing ? Green : Pink);
    if (connected) {
        drawText(media.mTitle.empty() ? (playing ? "Playing" : "Paused") : media.mTitle.c_str(),58,TFT_WHITE,0);
        drawText(media.mArtist.empty() ? "Unknown artist" : media.mArtist.c_str(),101,Cyan,1);
        drawText(media.mAlbum.c_str(),129,Muted,2);
    } else {
        sprite.drawCircle(CYD::Width/2,75,16,Cyan);
        sprite.drawCircle(CYD::Width/2,75,11,Pink);
        sprite.fillCircle(CYD::Width/2,75,4,Green);
        sprite.setTextDatum(MC_DATUM); sprite.loadFont(VietnameseSans28);
        sprite.setTextColor(TFT_WHITE,Card); sprite.drawString("CTRL 01",CYD::Width/2,113);
        sprite.loadFont(VietnameseSans20); sprite.setTextColor(Muted,Card);
        sprite.drawString("Chờ kết nối",CYD::Width/2,146);
        sprite.setTextDatum(TL_DATUM);
    }
    sprite.unloadFont(); sprite.setTextFont(1); sprite.setTextSize(1);
    sprite.setTextColor(Muted,Background);
    sprite.drawString(connected ? timeLabel(elapsedTime) : "Waiting for iPhone",12,CYD::ProgressY-12);
    sprite.setTextDatum(TR_DATUM);
    sprite.drawString(connected && media.mDuration>0 ? timeLabel(media.mDuration) : "--:--",CYD::Width-12,CYD::ProgressY-12);
    sprite.setTextDatum(TL_DATUM);
    sprite.fillRoundRect(12,CYD::ProgressY,CYD::Width-24,6,3,Track);
    if (connected && std::isfinite(media.mDuration) && media.mDuration>0 && std::isfinite(elapsedTime)) {
        const float ratio = constrain(elapsedTime/media.mDuration,0.0f,1.0f);
        const int progress = int(ratio*(CYD::Width-24));
        if (progress>0) sprite.fillRoundRect(12,CYD::ProgressY,progress,6,3,Cyan);
        sprite.fillCircle(12+progress,CYD::ProgressY+3,3,Green);
    }
    drawControls(playing,connected,highlighted);
    // Keep the raw SPI output that has been verified on this CYD board.
    uint16_t line[CYD::Width];
    for (int y = 0; y < CYD::Height; ++y) {
        for (int x = 0; x < CYD::Width; ++x) line[x] = sprite.readPixel(x,y);
        panel.sendRow(y,line);
    }
    if (waking) ledcWrite(0,127);
}
