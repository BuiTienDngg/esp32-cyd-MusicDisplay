#line 1 "D:\\My_project\\esp32_CYD_test\\cyd-port\\BLEHUDNaviESP32\\TFT_CYD.cpp"
#include "TFT_CYD.h"

namespace {
constexpr int PIN_MISO = 12, PIN_MOSI = 13, PIN_CLK = 14;
constexpr int PIN_CS = 15, PIN_DC = 2, PIN_BL = 21;
constexpr uint32_t SPI_HZ = 27000000;
// ILI9341 landscape MADCTL: MV | BGR; use 0xE8 for the opposite landscape.
constexpr uint8_t LANDSCAPE = 0x28;
}

void TFT_CYD::Command(uint8_t command, const uint8_t* data, size_t count) {
    m_spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    digitalWrite(PIN_DC, LOW);
    m_spi.transfer(command);
    digitalWrite(PIN_DC, HIGH);
    for (size_t i = 0; i < count; ++i) m_spi.transfer(data[i]);
    digitalWrite(PIN_CS, HIGH);
    m_spi.endTransaction();
}

void TFT_CYD::Init() {
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, LOW);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    pinMode(PIN_DC, OUTPUT);
    // The panel reset is connected to the board reset, not a separate GPIO.
    m_spi.begin(PIN_CLK, PIN_MISO, PIN_MOSI, PIN_CS);
    Command(0x01); // Software reset
    delay(150);
    // CYD alternative ILI9341 initialization: command, length, data.
    static const uint8_t init[] = {
        0xCF,3,0x00,0xC1,0x30, 0xED,4,0x64,0x03,0x12,0x81,
        0xE8,3,0x85,0x00,0x78, 0xCB,5,0x39,0x2C,0x00,0x34,0x02,
        0xF7,1,0x20, 0xEA,2,0x00,0x00,
        0xC0,1,0x10, 0xC1,1,0x00, 0xC5,2,0x30,0x30, 0xC7,1,0xB7,
        0x3A,1,0x55, 0x36,1,LANDSCAPE,
        0xB1,2,0x00,0x1A, 0xB6,3,0x08,0x82,0x27,
        0xF2,1,0x00, 0x26,1,0x01,
        0xE0,15,0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0,0,0,0,
        0xE1,15,0,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F
    };
    for (size_t i = 0; i < sizeof(init);) {
        uint8_t cmd = init[i++];
        uint8_t count = init[i++];
        Command(cmd, init + i, count);
        i += count;
    }
    Command(0x20); // Inversion off
    Command(0x11); // Sleep out
    delay(120);
    // Clear panel RAM before enabling the backlight.
    static const uint16_t blackRow[WIDTH] = {};
    for (int y = 0; y < HEIGHT; ++y) SendImage(0, y, WIDTH, 1, blackRow);
    Command(0x29); // Display on
    delay(20);
    digitalWrite(PIN_BL, HIGH);
}

void TFT_CYD::SendImage(int x, int y, int width, int height, const uint16_t* data) {
    if (!data || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > WIDTH || y + height > HEIGHT) return;
    const int x0 = x * 2, x1 = (x + width) * 2 - 1;
    const int y0 = y * 2, y1 = (y + height) * 2 - 1;
    const uint8_t columns[] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
    const uint8_t rows[] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
    Command(0x2A, columns, sizeof(columns));
    Command(0x2B, rows, sizeof(rows));
    m_spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    digitalWrite(PIN_DC, LOW);
    m_spi.transfer(0x2C);
    digitalWrite(PIN_DC, HIGH);
    uint8_t line[WIDTH * 4]; // Big-endian RGB565, two output pixels per source pixel.
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            uint16_t pixel = data[row * width + col];
            line[col * 4] = line[col * 4 + 2] = pixel >> 8;
            line[col * 4 + 1] = line[col * 4 + 3] = pixel & 0xFF;
        }
        m_spi.writeBytes(line, width * 4);
        m_spi.writeBytes(line, width * 4);
    }
    digitalWrite(PIN_CS, HIGH);
    m_spi.endTransaction();
}

void TFT_CYD::EnterSleepMode() {
    digitalWrite(PIN_BL, LOW);
    Command(0x28);
    Command(0x10);
    delay(120);
}
