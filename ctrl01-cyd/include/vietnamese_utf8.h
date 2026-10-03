#pragma once
#include <Arduino.h>
// Compose canonical Vietnamese combining sequences into supported Unicode glyphs.
String normalizeVietnameseUtf8(const String& text);
