// ESP32-2432S028R / ILI9341 / XPT2046 calibration and shared NVS mapping.
// Upload environment esp32-cyd-calibration, then esp32-cyd to restore CTRL 01.
#include <Arduino.h>
#include <Preferences.h>
#include <cmath>

namespace {
struct TouchCalibration {
    float ax, bx, cx, ay, by, cy;
};
TouchCalibration calibration{};
bool calibrationLoaded = false, calibrationAvailable = false;

bool loadCalibration(int rotation, int width, int height) {
    Preferences prefs;
    if (!prefs.begin("cyd-touch", true)) return false;
    const bool valid = prefs.getBool("valid", false) &&
        prefs.getInt("rotation", -1) == rotation &&
        prefs.getInt("width", 0) == width && prefs.getInt("height", 0) == height;
    if (valid) {
        calibration = {prefs.getFloat("ax"), prefs.getFloat("bx"), prefs.getFloat("cx"),
                       prefs.getFloat("ay"), prefs.getFloat("by"), prefs.getFloat("cy")};
    }
    prefs.end();
    return valid && std::isfinite(calibration.ax) && std::isfinite(calibration.bx) &&
        std::isfinite(calibration.cx) && std::isfinite(calibration.ay) &&
        std::isfinite(calibration.by) && std::isfinite(calibration.cy) &&
        fabsf(calibration.ax * calibration.by - calibration.bx * calibration.ay) > 1e-8f;
}
}

// Also compiled into the normal app. Handles swapped and reversed touch axes.
bool mapCalibratedTouch(uint16_t rawX, uint16_t rawY, int rotation,
                        int width, int height, int& x, int& y) {
    if (!calibrationLoaded) {
        calibrationAvailable = loadCalibration(rotation, width, height);
        calibrationLoaded = true;
        Serial.println(calibrationAvailable ? "Touch: using saved calibration" : "Touch: default endpoints (not calibrated)");
    }
    if (!calibrationAvailable) return false;
    x = constrain(int(lroundf(calibration.ax * rawX + calibration.bx * rawY + calibration.cx)), 0, width - 1);
    y = constrain(int(lroundf(calibration.ay * rawX + calibration.by * rawY + calibration.cy)), 0, height - 1);
    return true;
}

#ifdef CYD_TOUCH_CALIBRATION
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "cyd_board.h"

namespace {
constexpr int W = CYD::Width, H = CYD::Height, Rotation = CYD::Rotation;
constexpr int Margin = 24, Samples = 16;
SPIClass lcdSPI{HSPI}, touchSPI{VSPI};
XPT2046_Touchscreen touch{33, 36};
TFT_eSPI canvasOwner(240, 320);
TFT_eSprite canvas(&canvasOwner);
struct Point { float x, y; };
bool ready = false;

void lcdCommand(uint8_t cmd, const uint8_t* bytes = nullptr, size_t count = 0) {
    lcdSPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(15, LOW); digitalWrite(2, LOW);
    lcdSPI.transfer(cmd); digitalWrite(2, HIGH);
    for (size_t i = 0; i < count; ++i) lcdSPI.transfer(bytes[i]);
    digitalWrite(15, HIGH); lcdSPI.endTransaction();
}
void initPanel() {
    pinMode(21, OUTPUT); digitalWrite(21, LOW);
    pinMode(15, OUTPUT); digitalWrite(15, HIGH); pinMode(2, OUTPUT);
    lcdSPI.begin(14, 12, 13, 15);
    lcdCommand(0x01); delay(150);
    static const uint8_t init[] = {
        0xCF,3,0,0xC1,0x30, 0xED,4,0x64,3,0x12,0x81,
        0xE8,3,0x85,0,0x78, 0xCB,5,0x39,0x2C,0,0x34,2,
        0xF7,1,0x20, 0xEA,2,0,0, 0xC0,1,0x10, 0xC1,1,0,
        0xC5,2,0x30,0x30, 0xC7,1,0xB7, 0x3A,1,0x55,
        0xB1,2,0,0x1A, 0xB6,3,8,0x82,0x27, 0xF2,1,0, 0x26,1,1,
        0xE0,15,0x0F,0x2A,0x28,8,0x0E,8,0x54,0xA9,0x43,0x0A,0x0F,0,0,0,0,
        0xE1,15,0,0x15,0x17,7,0x11,6,0x2B,0x56,0x3C,5,0x10,0x0F,0x3F,0x3F,0x0F
    };
    for (size_t i = 0; i < sizeof(init);) {
        const uint8_t cmd = init[i++], count = init[i++];
        lcdCommand(cmd, init + i, count); i += count;
    }
    static const uint8_t madctl[] = {0x48,0x28,0x88,0xE8};
    lcdCommand(0x36, &madctl[Rotation], 1);
    lcdCommand(0x20); lcdCommand(0x11); delay(120);
    lcdCommand(0x29); delay(20);
}
void present() {
    uint8_t row[W * 2];
    const uint8_t columns[] = {0,0,uint8_t((W-1) >> 8),uint8_t(W-1)};
    for (int y = 0; y < H; ++y) {
        const uint8_t rows[] = {uint8_t(y >> 8),uint8_t(y),uint8_t(y >> 8),uint8_t(y)};
        lcdCommand(0x2A, columns, 4); lcdCommand(0x2B, rows, 4);
        for (int x = 0; x < W; ++x) {
            const uint16_t pixel = canvas.readPixel(x, y);
            row[x * 2] = pixel >> 8; row[x * 2 + 1] = pixel;
        }
        lcdSPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
        digitalWrite(15, LOW); digitalWrite(2, LOW); lcdSPI.transfer(0x2C);
        digitalWrite(2, HIGH); lcdSPI.writeBytes(row, sizeof(row));
        digitalWrite(15, HIGH); lcdSPI.endTransaction();
    }
    digitalWrite(21, HIGH);
}
void text(const char* value, int x, int y, uint16_t color = TFT_WHITE) {
    canvas.setTextColor(color, TFT_BLACK); canvas.drawString(value, x, y);
}
void target(int index, int x, int y) {
    canvas.fillSprite(TFT_BLACK);
    const int titleY = index < 4 ? H / 2 - 28 : 20;
    text("TOUCH CALIBRATION", 55, titleY, TFT_CYAN);
    text("Hold the WHITE cross", 45, titleY + 20);
    text("Release after each point", 35, titleY + 40);
    text((index < 4 ? "Point " + String(index + 1) + "/4" : String("Verify CENTER")).c_str(), 85, titleY + 60);
    canvas.drawCircle(x, y, 10, TFT_WHITE);
    canvas.drawFastHLine(x - 15, y, 31, TFT_WHITE);
    canvas.drawFastVLine(x, y - 15, 31, TFT_WHITE);
    present();
}
void waitRelease() {
    uint32_t released = millis();
    while (millis() - released < 180) {
        if (touch.touched()) released = millis();
        delay(10);
    }
}
bool sample(Point& point) {
    const uint32_t started = millis();
    int16_t xs[Samples], ys[Samples];
    while (millis() - started < 45000) {
        if (!touch.touched()) { delay(10); continue; }
        delay(40); // Allow initial contact to settle.
        int count = 0;
        while (count < Samples && touch.touched()) {
            const TS_Point p = touch.getPoint();
            if (p.x > 0 && p.x < 4095 && p.y > 0 && p.y < 4095) {
                xs[count] = p.x; ys[count] = p.y; ++count;
            }
            delay(12);
        }
        if (count != Samples) { waitRelease(); continue; }
        for (int i = 1; i < Samples; ++i) {
            for (int j = i; j > 0 && xs[j] < xs[j-1]; --j) { int16_t v = xs[j]; xs[j] = xs[j-1]; xs[j-1] = v; }
            for (int j = i; j > 0 && ys[j] < ys[j-1]; --j) { int16_t v = ys[j]; ys[j] = ys[j-1]; ys[j-1] = v; }
        }
        if (xs[12] - xs[3] > 150 || ys[12] - ys[3] > 150) {
            Serial.println("Unstable contact: release and hold target again");
            waitRelease(); continue;
        }
        point = {(xs[7] + xs[8]) / 2.0f, (ys[7] + ys[8]) / 2.0f};
        Serial.printf("Raw X=%.1f Y=%.1f\n", point.x, point.y);
        canvas.fillRect(30, H/2 + 52, W-60, 18, TFT_BLACK);
        text("OK - RELEASE NOW", 55, H/2 + 52, TFT_GREEN);
        present();
        waitRelease(); return true;
    }
    return false;
}
Point mapped(Point raw) {
    return {calibration.ax * raw.x + calibration.bx * raw.y + calibration.cx,
            calibration.ay * raw.x + calibration.by * raw.y + calibration.cy};
}
bool save() {
    Preferences prefs;
    if (!prefs.begin("cyd-touch", false)) return false;
    prefs.putBool("valid", false);
    bool ok = prefs.putInt("rotation", Rotation) == 4 && prefs.putInt("width", W) == 4 && prefs.putInt("height", H) == 4;
    ok = (prefs.putFloat("ax", calibration.ax) == 4) && ok;
    ok = (prefs.putFloat("bx", calibration.bx) == 4) && ok;
    ok = (prefs.putFloat("cx", calibration.cx) == 4) && ok;
    ok = (prefs.putFloat("ay", calibration.ay) == 4) && ok;
    ok = (prefs.putFloat("by", calibration.by) == 4) && ok;
    ok = (prefs.putFloat("cy", calibration.cy) == 4) && ok;
    if (ok) ok = prefs.putBool("valid", true) == 1;
    prefs.end(); return ok;
}
bool calibrate() {
    Point raw[4];
    const Point screen[] = {{Margin,Margin}, {W-1-Margin,Margin},
        {W-1-Margin,H-1-Margin}, {Margin,H-1-Margin}};
    for (int i = 0; i < 4; ++i) {
        waitRelease();
        target(i, screen[i].x, screen[i].y);
        if (!sample(raw[i])) return false;
    }
    const Point horizontal = {(raw[1].x+raw[2].x-raw[0].x-raw[3].x)/2,
                              (raw[1].y+raw[2].y-raw[0].y-raw[3].y)/2};
    const Point vertical = {(raw[2].x+raw[3].x-raw[0].x-raw[1].x)/2,
                            (raw[2].y+raw[3].y-raw[0].y-raw[1].y)/2};
    const float det = horizontal.x * vertical.y - horizontal.y * vertical.x;
    if (fabsf(det) < 100000) { Serial.println("Invalid axes: touch all four different targets"); return false; }
    const float sx = W-1-2*Margin, sy = H-1-2*Margin;
    calibration = {sx*vertical.y/det, -sx*vertical.x/det, 0,
                   -sy*horizontal.y/det, sy*horizontal.x/det, 0};
    const float mx = (raw[0].x+raw[1].x+raw[2].x+raw[3].x)/4;
    const float my = (raw[0].y+raw[1].y+raw[2].y+raw[3].y)/4;
    calibration.cx = (W-1)/2.0f - calibration.ax*mx - calibration.bx*my;
    calibration.cy = (H-1)/2.0f - calibration.ay*mx - calibration.by*my;
    for (int i = 0; i < 4; ++i) {
        const Point p = mapped(raw[i]);
        if (hypotf(p.x-screen[i].x, p.y-screen[i].y) > 12) {
            Serial.println("Corner accuracy failed; use a stylus and retry"); return false;
        }
    }
    waitRelease();
    target(4, W/2, H/2);
    Serial.println("Verify calibration: touch the center cross");
    Point center;
    if (!sample(center)) return false;
    const Point check = mapped(center);
    if (hypotf(check.x-W/2, check.y-H/2) > 15) {
        Serial.println("Center verification failed; retry"); return false;
    }
    if (!save()) { Serial.println("NVS save failed"); return false; }
    Serial.printf("SAVED rotation=%d, %dx%d\n", Rotation, W, H);
    Serial.printf("x = %.9f * rawX + %.9f * rawY + %.9f\n", calibration.ax, calibration.bx, calibration.cx);
    Serial.printf("y = %.9f * rawX + %.9f * rawY + %.9f\n", calibration.ay, calibration.by, calibration.cy);
    return true;
}
void testScreen(int x = -1, int y = -1) {
    canvas.fillSprite(TFT_BLACK);
    text(ready ? "CALIBRATION SAVED" : "CALIBRATION FAILED", 45, 25, ready ? TFT_GREEN : TFT_RED);
    text("BOOT or serial R: recalibrate", 10, 48);
    text("Tap all 5 buttons to verify", 10, 70);
    text("Then upload esp32-cyd", 10, 90);
    if (x >= 0) text(("X=" + String(x) + " Y=" + String(y)).c_str(), 10, 120, TFT_YELLOW);
    const char* labels[] = {"PREV", "PLAY", "NEXT", "VOL-", "VOL+"};
    for (int i = 0; i < 5; ++i) {
        const int left = i * CYD::ControlWidth;
        const bool hit = x >= left && x < left + CYD::ControlWidth && y >= CYD::ControlsY && y < CYD::ControlsY + CYD::ControlHeight;
        canvas.fillRoundRect(left+2, CYD::ControlsY, CYD::ControlWidth-4, CYD::ControlHeight, 4, hit ? TFT_GREEN : TFT_DARKGREY);
        canvas.setTextColor(TFT_WHITE, hit ? TFT_GREEN : TFT_DARKGREY);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString(labels[i], left+CYD::ControlWidth/2, CYD::ControlsY+CYD::ControlHeight/2);
        canvas.setTextDatum(TL_DATUM);
    }
    if (x >= 0) canvas.drawCircle(x, y, 5, TFT_YELLOW);
    present();
}
}

void setup() {
    Serial.begin(115200);
    pinMode(0, INPUT_PULLUP);
    canvas.setColorDepth(8); canvas.setTextWrap(false, false);
    if (!canvas.createSprite(W, H)) { Serial.println("Sprite allocation failed"); while (true) delay(1000); }
    initPanel();
    touchSPI.begin(25,39,32,33); touch.begin(touchSPI); touch.setRotation(Rotation);
    Serial.printf("CYD TOUCH CALIBRATION rotation=%d, %dx%d. Use stylus; hold each cross.\n", Rotation, W, H);
    ready = loadCalibration(Rotation, W, H);
    if (!ready) ready = calibrate();
    testScreen();
}
void loop() {
    if (digitalRead(0) == LOW || (Serial.available() && (Serial.read() & ~0x20) == 'R')) {
        while (digitalRead(0) == LOW) delay(10);
        ready = calibrate(); testScreen();
    }
    if (ready && touch.touched()) {
        const TS_Point raw = touch.getPoint();
        const Point p = mapped({float(raw.x), float(raw.y)});
        const int x = constrain(int(lroundf(p.x)), 0, W-1), y = constrain(int(lroundf(p.y)), 0, H-1);
        Serial.printf("RAW %d,%d -> PIXEL %d,%d pressure=%d\n", raw.x, raw.y, x, y, raw.z);
        testScreen(x, y);
    }
    delay(30);
}
#endif // CYD_TOUCH_CALIBRATION
