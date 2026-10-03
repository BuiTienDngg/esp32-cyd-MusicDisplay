#include "vietnamese_utf8.h"
#include "fonts/VietnameseCompositions.h"

namespace {
uint32_t decode(const String& text, unsigned& index) {
    const uint8_t first = text[index++];
    if (first < 0x80) return first;
    unsigned count;
    uint32_t code;
    if ((first & 0xE0) == 0xC0) { count = 1; code = first & 0x1F; }
    else if ((first & 0xF0) == 0xE0) { count = 2; code = first & 0x0F; }
    else if ((first & 0xF8) == 0xF0) { count = 3; code = first & 7; }
    else return '?';
    for (unsigned i = 0; i < count; ++i) {
        if (index >= text.length() || (uint8_t(text[index]) & 0xC0) != 0x80) return '?';
        code = (code << 6) | (uint8_t(text[index++]) & 0x3F);
    }
    return code;
}
void append(String& text, uint32_t code) {
    if (code < 0x80) text += char(code);
    else if (code < 0x800) {
        text += char(0xC0 | (code >> 6));
        text += char(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        text += char(0xE0 | (code >> 12));
        text += char(0x80 | ((code >> 6) & 0x3F));
        text += char(0x80 | (code & 0x3F));
    } else {
        text += char(0xF0 | (code >> 18));
        text += char(0x80 | ((code >> 12) & 0x3F));
        text += char(0x80 | ((code >> 6) & 0x3F));
        text += char(0x80 | (code & 0x3F));
    }
}
int combiningClass(uint32_t code) {
    if (code == 0x031B) return 216; // horn
    if (code == 0x0323) return 220; // dot below
    if (code == 0x0300 || code == 0x0301 || code == 0x0302 || code == 0x0303 ||
        code == 0x0306 || code == 0x0309 || code == 0x0308 || code == 0x030A || code == 0x0327)
        return code == 0x0327 ? 202 : 230;
    return 0;
}
VietnameseComposition entry(unsigned i) {
    VietnameseComposition value;
    memcpy_P(&value, &VietnameseCompositions[i], sizeof(value));
    return value;
}
constexpr unsigned Count = sizeof(VietnameseCompositions) / sizeof(VietnameseCompositions[0]);
}

String normalizeVietnameseUtf8(const String& text) {
    String result;
    result.reserve(text.length());
    unsigned index = 0;
    while (index < text.length()) {
        uint32_t base = decode(text, index);
        uint16_t marks[2] = {};
        unsigned count = 0;
        // Expand an already composed base before applying any additional marks.
        for (unsigned i = 0; i < Count; ++i) {
            const auto e = entry(i);
            if (e.composed == base) {
                base = e.base;
                marks[count++] = e.first;
                if (e.second) marks[count++] = e.second;
                break;
            }
        }
        while (index < text.length() && count < 2) {
            unsigned next = index;
            const uint32_t mark = decode(text, next);
            if (!combiningClass(mark)) break;
            marks[count++] = mark;
            index = next;
        }
        if (count == 2 && combiningClass(marks[0]) > combiningClass(marks[1])) {
            const uint16_t first = marks[0]; marks[0] = marks[1]; marks[1] = first;
        }
        bool composed = false;
        for (unsigned i = 0; count && i < Count; ++i) {
            const auto e = entry(i);
            if (e.base == base && e.first == marks[0] && e.second == marks[1]) {
                append(result, e.composed);
                composed = true;
                break;
            }
        }
        if (!composed) {
            append(result, base);
            for (unsigned i = 0; i < count; ++i) append(result, marks[i]);
        }
    }
    return result;
}
