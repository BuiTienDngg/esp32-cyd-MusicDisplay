#pragma once
#include <Arduino.h>
#include <atomic>
#include <OneButton.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "cyd_board.h"
class ButtonManager {
public:
    void begin();
    void handle();
    bool isAnyClicked() const;
    bool nextCommand(uint8_t& command);
    void clearCommands();
    int highlightedControl() const;
private:
    OneButton boot{CYD::Boot, true, true};
    SPIClass touchSPI{VSPI};
    XPT2046_Touchscreen touch{CYD::TouchCs, CYD::TouchIrq};
    QueueHandle_t commands = nullptr;
    bool touchPressed = false;
    uint32_t lastLongCommand = 0;
    std::atomic<int> heldControl{-1};
    // 29-bit timestamp + 3-bit (button index + 1), published as one atomic value.
    std::atomic<uint32_t> feedback{0};
    void flashControl(int index);
    void enqueue(uint8_t command);
};
