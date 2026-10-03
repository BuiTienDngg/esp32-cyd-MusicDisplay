from pathlib import Path
p = Path(__file__).parent / 'BLEHUDNaviESP32'
f = p / 'BLEHUDNaviESP32.ino'
s = f.read_text()
s = s.replace('#include "VoltageMeasurement.h"', '#include <esp_sleep.h>')
a = s.index('// -----------------\n// Display selection')
b = s.index('// ---------------------\n// Variables for display', a)
s = s[:a] + '// ESP32-2432S028R: ILI9341 landscape 320x240, 2x logical canvas.\n#include "TFT_CYD.h"\nTFT_CYD selectedDisplay;\n\n' + s[b:]
s = s.replace('const int CANVAS_WIDTH = g_display.GetWidth();', 'constexpr int CANVAS_WIDTH = TFT_CYD::WIDTH;')
s = s.replace('const int CANVAS_HEIGHT = g_display.GetHeight();', 'constexpr int CANVAS_HEIGHT = TFT_CYD::HEIGHT;')
s = s.replace('uint16_t* g_canvas = NULL;', 'uint16_t g_canvas[CANVAS_WIDTH * CANVAS_HEIGHT] = {};')
a = s.index('#define TTGO_LEFT_BUTTON')
b = s.index('// ---------------------\n// Bluetooth event callbacks', a)
s = s[:a] + '''// Hold BOOT for 500 ms, then release to sleep. BOOT wakes the device.
#define BUTTON_DEEP_SLEEP 0
#define GPIO_NUM_WAKEUP GPIO_NUM_0
Button2 g_btnDeepSleep(BUTTON_DEEP_SLEEP);
bool g_sleepRequested = false;

''' + s[b:]
s = s.replace('std::string value = pCharacteristic->getValue();', '''// ESP32 core 2.x returns std::string; core 3.x returns Arduino String.
        const auto received = pCharacteristic->getValue();
        std::string value(received.c_str(), received.length());''')
s = s.replace('sprintf(tmp, "%02X ", value[i]);', 'snprintf(tmp, sizeof(tmp), "%02X ", static_cast<unsigned char>(value[i]));')
s = s.replace('    g_canvas = new uint16_t[CANVAS_WIDTH * CANVAS_HEIGHT];\n', '')
s = s.replace('Draw4bitImageProgmem(0, 32, 128, 64, IMG_logoTbt128x64_4b);', 'Draw4bitImageProgmem((CANVAS_WIDTH - 128) / 2, (CANVAS_HEIGHT - 64) / 2, 128, 64, IMG_logoTbt128x64_4b);')
a = s.index('        // without this the module')
b = s.index('        esp_sleep_enable_ext0_wakeup', a)
s = s[:a] + s[b:]
a = s.index('    // setup voltage measurement')
b = s.index('    Serial.println("setup() finished");', a)
s = s[:a] + s[b:]
s = s.replace('    g_btn1.loop();\n', '')
a = s.index('    else if (g_showVoltage)')
b = s.index('    else if (g_deviceConnected)', a)
s = s[:a] + s[b:]
# Center the original 128-pixel icon pair on the wider logical canvas.
s = s.replace('Draw4bitImageProgmem(0, 0, 64, 64, imageProgmem);', 'Draw4bitImageProgmem((CANVAS_WIDTH - 128) / 2, 0, 64, 64, imageProgmem);')
s = s.replace('DrawImageProgmem(64, 0, 64, 64,', 'DrawImageProgmem((CANVAS_WIDTH - 128) / 2 + 64, 0, 64, 64,')
s = s.replace('DrawMessage(str, 64 + x,', 'DrawMessage(str, (CANVAS_WIDTH - 128) / 2 + 64 + x,')
s = s.replace('CANVAS_WIDTH - x;', 'CANVAS_WIDTH - 1 - x;').replace('CANVAS_WIDTH - y;', 'CANVAS_WIDTH - 1 - y;')
s = s.replace('CANVAS_HEIGHT - y;', 'CANVAS_HEIGHT - 1 - y;').replace('CANVAS_HEIGHT - xOriginal;', 'CANVAS_HEIGHT - 1 - xOriginal;')
f.write_text(s)
# Arduino compiles every .cpp in the sketch folder, even unused adapters.
f = p / 'TFT_TTGO.cpp'
f.write_text('#if defined(BLEHUD_ENABLE_TTGO)\n' + f.read_text() + '\n#endif // BLEHUD_ENABLE_TTGO\n')
