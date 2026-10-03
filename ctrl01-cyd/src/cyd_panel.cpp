#include "cyd_panel.h"

namespace {
constexpr int Cs = 15, Dc = 2;
constexpr uint32_t Frequency = 10000000;
}
void CydPanel::command(uint8_t value, const uint8_t* data, size_t count) {
    spi.beginTransaction(SPISettings(Frequency, MSBFIRST, SPI_MODE0));
    digitalWrite(Cs, LOW);
    digitalWrite(Dc, LOW);
    spi.transfer(value);
    digitalWrite(Dc, HIGH);
    for (size_t i = 0; i < count; ++i) spi.transfer(data[i]);
    digitalWrite(Cs, HIGH);
    spi.endTransaction();
}

void CydPanel::begin() {
    ledcWrite(0, 0);
    pinMode(Cs, OUTPUT);
    digitalWrite(Cs, HIGH);
    pinMode(Dc, OUTPUT);
    spi.begin(14, 12, 13, Cs);
    command(0x01);
    delay(150);
    static const uint8_t init[] = {
        0xCF,3,0x00,0xC1,0x30, 0xED,4,0x64,0x03,0x12,0x81,
        0xE8,3,0x85,0x00,0x78, 0xCB,5,0x39,0x2C,0x00,0x34,0x02,
        0xF7,1,0x20, 0xEA,2,0x00,0x00,
        0xC0,1,0x10, 0xC1,1,0x00, 0xC5,2,0x30,0x30, 0xC7,1,0xB7,
        0x3A,1,0x55, // 16-bit RGB565, matching the output byte stream.
        0xB1,2,0x00,0x1A, 0xB6,3,0x08,0x82,0x27,
        0xF2,1,0x00, 0x26,1,0x01,
        0xE0,15,0x0F,0x2A,0x28,0x08,0x0E,0x08,0x54,0xA9,0x43,0x0A,0x0F,0,0,0,0,
        0xE1,15,0,0x15,0x17,0x07,0x11,0x06,0x2B,0x56,0x3C,0x05,0x10,0x0F,0x3F,0x3F,0x0F
    };
    for (size_t i = 0; i < sizeof(init);) {
        const uint8_t cmd = init[i++];
        const uint8_t count = init[i++];
        command(cmd, init + i, count);
        i += count;
    }
    static const uint8_t rotations[] = {0x48, 0x28, 0x88, 0xE8};
    command(0x36, &rotations[CYD::Rotation], 1);
    command(0x20);
    command(0x11);
    delay(120);
    static const uint16_t black[CYD::Width] = {};
    for (int y = 0; y < CYD::Height; ++y) sendRow(y, black);
    command(0x29);
    delay(20);
    ledcWrite(0, 127);
    Serial.printf("CYD RAW SPI 10MHz RGB565: rotation=%d, %dx%d\n", CYD::Rotation, CYD::Width, CYD::Height);
}

void CydPanel::sendRow(int y, const uint16_t* pixels) {
    if (!pixels || y < 0 || y >= CYD::Height) return;
    constexpr int last = CYD::Width - 1;
    const uint8_t columns[] = {0, 0, uint8_t(last >> 8), uint8_t(last)};
    const uint8_t rows[] = {uint8_t(y >> 8), uint8_t(y), uint8_t(y >> 8), uint8_t(y)};
    command(0x2A, columns, 4);
    command(0x2B, rows, 4);
    uint8_t bytes[CYD::Width * 2];
    for (int x = 0; x < CYD::Width; ++x) {
        bytes[x * 2] = pixels[x] >> 8;
        bytes[x * 2 + 1] = pixels[x] & 0xFF;
    }
    spi.beginTransaction(SPISettings(Frequency, MSBFIRST, SPI_MODE0));
    digitalWrite(Cs, LOW);
    digitalWrite(Dc, LOW);
    spi.transfer(0x2C);
    digitalWrite(Dc, HIGH);
    spi.writeBytes(bytes, sizeof(bytes));
    digitalWrite(Cs, HIGH);
    spi.endTransaction();
}
void CydPanel::sleep() { command(0x28); command(0x10); }
void CydPanel::wake() { command(0x11); delay(120); command(0x29); }
