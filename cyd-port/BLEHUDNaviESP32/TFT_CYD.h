#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "IDisplay.h"

// ESP32-2432S028R, ILI9341. Logical pixels are doubled in both axes.
class TFT_CYD : public IDisplay {
public:
    static constexpr int WIDTH = 160;
    static constexpr int HEIGHT = 120;
    void Init() override;
    int GetWidth() override { return WIDTH; }
    int GetHeight() override { return HEIGHT; }
    void SendImage(int x, int y, int width, int height, const uint16_t* data) override;
    void EnterSleepMode() override;
private:
    SPIClass m_spi{HSPI};
    void Command(uint8_t command, const uint8_t* data = nullptr, size_t count = 0);
};
