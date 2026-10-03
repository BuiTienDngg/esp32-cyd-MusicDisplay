#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "cyd_board.h"

// Hardware output only; TFT_eSprite remains responsible for drawing and fonts.
class CydPanel {
public:
    void begin();
    void sleep();
    void wake();
    void sendRow(int y, const uint16_t* pixels);
private:
    SPIClass spi{HSPI};
    void command(uint8_t value, const uint8_t* data = nullptr, size_t count = 0);
};
