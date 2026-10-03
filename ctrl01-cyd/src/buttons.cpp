#include "buttons.h"
#include "apple_media_service.h"
// Implemented in touch_calibration.cpp; saved profile is shared with the calibrator.
bool mapCalibratedTouch(uint16_t rawX, uint16_t rawY, int rotation,
                        int width, int height, int& x, int& y);

void ButtonManager::enqueue(uint8_t command) {
    switch (command) {
        case RemoteCommandIDPreviousTrack: flashControl(0); break;
        case RemoteCommandIDTogglePlayPause: flashControl(1); break;
        case RemoteCommandIDNextTrack: flashControl(2); break;
        case RemoteCommandIDVolumeDown: flashControl(3); break;
        case RemoteCommandIDVolumeUp: flashControl(4); break;
    }
    if (commands) xQueueSend(commands, &command, 0);
}
void ButtonManager::flashControl(int index) {
    feedback.store((millis() << 3) | uint32_t(index + 1));
}
int ButtonManager::highlightedControl() const {
    const int held = heldControl.load();
    if (held >= 0) return held;
    const uint32_t event = feedback.load();
    const uint32_t age = ((millis() & 0x1FFFFFFF) - (event >> 3)) & 0x1FFFFFFF;
    return (event & 7) && age < 550 ? int(event & 7) - 1 : -1;
}
void ButtonManager::begin() {
    commands = xQueueCreate(8, sizeof(uint8_t));
    configASSERT(commands);
    boot.setDebounceMs(25);
    boot.setClickMs(250);
    boot.setPressMs(500);
    boot.attachClick([](void* p) {
        static_cast<ButtonManager*>(p)->enqueue(RemoteCommandIDTogglePlayPause);
    }, this);
    boot.attachDoubleClick([](void* p) {
        static_cast<ButtonManager*>(p)->enqueue(RemoteCommandIDNextTrack);
    }, this);
    boot.attachMultiClick([](void* p) {
        auto self = static_cast<ButtonManager*>(p);
        if (self->boot.getNumberClicks() == 3) self->enqueue(RemoteCommandIDPreviousTrack);
    }, this);
    boot.attachDuringLongPress([](void* p) {
        auto self = static_cast<ButtonManager*>(p);
        if (millis() - self->lastLongCommand >= 200) {
            self->lastLongCommand = millis();
            self->enqueue(RemoteCommandIDVolumeDown);
        }
    }, this);
    touchSPI.begin(CYD::TouchClk, CYD::TouchMiso, CYD::TouchMosi, CYD::TouchCs);
    touch.begin(touchSPI);
    touch.setRotation(CYD::Rotation);
}
void ButtonManager::handle() {
    boot.tick();
    const bool down = touch.tirqTouched() && touch.touched();
    if (down && !touchPressed) {
        const TS_Point point = touch.getPoint();
        int x, y;
        if (!mapCalibratedTouch(point.x, point.y, CYD::Rotation, CYD::Width, CYD::Height, x, y)) {
            x = constrain(map(point.x, CYD::TouchMinX, CYD::TouchMaxX, 0, CYD::Width - 1), 0L, long(CYD::Width - 1));
            y = constrain(map(point.y, CYD::TouchMinY, CYD::TouchMaxY, 0, CYD::Height - 1), 0L, long(CYD::Height - 1));
        }
        if (y >= CYD::ControlsY && y < CYD::ControlsY + CYD::ControlHeight) {
            static const uint8_t action[] = {RemoteCommandIDPreviousTrack, RemoteCommandIDTogglePlayPause,
                RemoteCommandIDNextTrack, RemoteCommandIDVolumeDown, RemoteCommandIDVolumeUp};
            const int index = x / CYD::ControlWidth;
            heldControl.store(index);
            enqueue(action[index]);
        }
    }
    if (!down && touchPressed) {
        const int released = heldControl.exchange(-1);
        if (released >= 0) flashControl(released);
    }
    touchPressed = down;
}
bool ButtonManager::isAnyClicked() const {
    return touchPressed || digitalRead(CYD::Boot) == LOW;
}
bool ButtonManager::nextCommand(uint8_t& command) {
    return commands && xQueueReceive(commands, &command, 0) == pdTRUE;
}
void ButtonManager::clearCommands() {
    if (commands) xQueueReset(commands);
}
