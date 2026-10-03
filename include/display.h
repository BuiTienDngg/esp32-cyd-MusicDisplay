#pragma once

#include <Arduino.h>

void initDisplay();
void drawDisplayTestPattern();
void showDisplayMessage(const char* title, const char* detail = nullptr);
void drawDisplayImage(int x, int y, int width, int height,
                      const uint16_t* pixels); 