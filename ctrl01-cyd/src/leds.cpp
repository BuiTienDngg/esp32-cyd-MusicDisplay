#include "leds.h"
#include "cyd_board.h"
void LEDManager::begin() {
    // Active-low RGB GPIOs; GPIO15 is TFT CS and must not drive a NeoPixel.
    for (int pin : {CYD::LedRed, CYD::LedGreen, CYD::LedBlue}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
}
void LEDManager::updateLEDs(AppleMediaService::MediaInformation::PlaybackState state, bool connected) {
    const bool playing = state == AppleMediaService::MediaInformation::PlaybackState::Playing;
    digitalWrite(CYD::LedRed, HIGH);
    digitalWrite(CYD::LedGreen, connected && !playing ? LOW : HIGH);
    digitalWrite(CYD::LedBlue, (connected ? playing : ((millis() / 500) % 2 == 0)) ? LOW : HIGH);
}
void LEDManager::disable() {
    digitalWrite(CYD::LedRed, HIGH);
    digitalWrite(CYD::LedGreen, HIGH);
    digitalWrite(CYD::LedBlue, HIGH);
}
