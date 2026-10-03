#include "cyd_tft.h"

namespace {
constexpr int PIN_MISO = 12;
constexpr int PIN_MOSI = 13;
constexpr int PIN_CLOCK = 14;
constexpr int PIN_CS = 15;
constexpr int PIN_DC = 2;
constexpr int PIN_BACKLIGHT = 21;
constexpr uint32_t CYD_SPI_HZ = 27000000;
constexpr uint8_t LANDSCAPE_MADCTL = 0x28;
}

void CydTft::command(uint8_t value, const uint8_t* data, size_t count) {
  spi.beginTransaction(SPISettings(CYD_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  spi.transfer(value);
  digitalWrite(PIN_DC, HIGH);
  for (size_t index = 0; index < count; ++index) {
    spi.transfer(data[index]);
  }
  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}

void CydTft::begin() {
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT);
  spi.begin(PIN_CLOCK, PIN_MISO, PIN_MOSI, PIN_CS);

  command(0x01);
  delay(150);

  static const uint8_t initCommands[] = {
      0xCF, 3, 0x00, 0xC1, 0x30, 0xED, 4, 0x64, 0x03, 0x12, 0x81,
      0xE8, 3, 0x85, 0x00, 0x78, 0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
      0xF7, 1, 0x20, 0xEA, 2, 0x00, 0x00, 0xC0, 1, 0x10, 0xC1, 1, 0x00,
      0xC5, 2, 0x30, 0x30, 0xC7, 1, 0xB7, 0x3A, 1, 0x55, 0x36, 1,
      LANDSCAPE_MADCTL, 0xB1, 2, 0x00, 0x1A, 0xB6, 3, 0x08, 0x82, 0x27,
      0xF2, 1, 0x00, 0x26, 1, 0x01, 0xE0, 15, 0x0F, 0x2A, 0x28, 0x08,
      0x0E, 0x08, 0x54, 0xA9, 0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00,
      0xE1, 15, 0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56, 0x3C,
      0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F};

  for (size_t index = 0; index < sizeof(initCommands);) {
    const uint8_t commandValue = initCommands[index++];
    const uint8_t dataLength = initCommands[index++];
    command(commandValue, initCommands + index, dataLength);
    index += dataLength;
  }

  command(0x20);
  command(0x11);
  delay(120);

  static const uint16_t blackRow[WIDTH] = {};
  for (int row = 0; row < HEIGHT; ++row) {
    sendImage(0, row, WIDTH, 1, blackRow);
  }

  command(0x29);
  delay(20);
  digitalWrite(PIN_BACKLIGHT, HIGH);
}

void CydTft::sendImage(int x, int y, int width, int height,
                       const uint16_t* pixels) {
  if (pixels == nullptr || x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > WIDTH || y + height > HEIGHT) {
    return;
  }

  const int xStart = x * 2;
  const int xEnd = (x + width) * 2 - 1;
  const int yStart = y * 2;
  const int yEnd = (y + height) * 2 - 1;

  const uint8_t columns[] = {
      static_cast<uint8_t>(xStart >> 8), static_cast<uint8_t>(xStart),
      static_cast<uint8_t>(xEnd >> 8), static_cast<uint8_t>(xEnd)};
  const uint8_t rows[] = {
      static_cast<uint8_t>(yStart >> 8), static_cast<uint8_t>(yStart),
      static_cast<uint8_t>(yEnd >> 8), static_cast<uint8_t>(yEnd)};

  command(0x2A, columns, sizeof(columns));
  command(0x2B, rows, sizeof(rows));

  spi.beginTransaction(SPISettings(CYD_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  spi.transfer(0x2C);
  digitalWrite(PIN_DC, HIGH);

  uint8_t line[WIDTH * 4];
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const uint16_t pixel = pixels[row * width + column];
      const uint8_t high = static_cast<uint8_t>(pixel >> 8);
      const uint8_t low = static_cast<uint8_t>(pixel);
      const int output = column * 4;
      line[output] = high;
      line[output + 1] = low;
      line[output + 2] = high;
      line[output + 3] = low;
    }
    spi.writeBytes(line, width * 4);
    spi.writeBytes(line, width * 4);
  }

  digitalWrite(PIN_CS, HIGH);
  spi.endTransaction();
}