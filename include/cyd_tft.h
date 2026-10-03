#pragma once

#include <Arduino.h>
#include <SPI.h>

class CydTft {
public:
  static constexpr int WIDTH = 160;
  static constexpr int HEIGHT = 120;

  void begin();
  void sendImage(int x, int y, int width, int height, const uint16_t* pixels);

private:
  SPIClass spi{HSPI};
  void command(uint8_t value, const uint8_t* data = nullptr, size_t count = 0);
};