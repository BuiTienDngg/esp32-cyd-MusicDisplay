#include "display.h"
#include "cyd_tft.h"

#include "config.h"

namespace {
CydTft tft;
}

void initDisplay() {
  pinMode(AppConfig::TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(AppConfig::TFT_BACKLIGHT_PIN, HIGH);
  tft.begin();
}

void drawDisplayTestPattern() {
  static uint16_t row[CydTft::WIDTH];
  for (int y = 0; y < CydTft::HEIGHT; ++y) {
    for (int x = 0; x < CydTft::WIDTH; ++x) {
      uint16_t color = 0;
      if (x < CydTft::WIDTH / 3) {
        color = 0xF800;  // red
      } else if (x < (CydTft::WIDTH * 2) / 3) {
        color = 0x07E0;  // green
      } else {
        color = 0x001F;  // blue
      }
      if ((x / 8 + y / 8) % 2 == 0) {
        color = 0xFFFF;  // white check squares
      }
      row[x] = color;
    }
    tft.sendImage(0, y, CydTft::WIDTH, 1, row);
  }
}

void showDisplayMessage(const char* title, const char* detail) {
  static const uint16_t blackRow[CydTft::WIDTH] = {};
  for (int row = 0; row < CydTft::HEIGHT; ++row) {
    tft.sendImage(0, row, CydTft::WIDTH, 1, blackRow);
  }
  if (title != nullptr) {
    for (int row = 0; row < 8; ++row) {
      tft.sendImage(0, 40 + row, CydTft::WIDTH, 1, blackRow);
    }
  }
  if (detail != nullptr) {
    for (int row = 0; row < 8; ++row) {
      tft.sendImage(0, 60 + row, CydTft::WIDTH, 1, blackRow);
    }
  }
}

void drawDisplayImage(int x, int y, int width, int height,
                      const uint16_t* pixels) {
  tft.sendImage(x, y, width, height, pixels);
}
