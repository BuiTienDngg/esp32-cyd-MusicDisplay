# BLE HUD on ESP32-2432S028R (ILI9341)

## Arduino IDE

- Open `BLEHUDNaviESP32.ino`.
- Board: **ESP32 Dev Module** (classic ESP32, not ESP32-S3).
- Flash Size: **4MB**; PSRAM: **Disabled**.
- Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**.
- Library: **Button2** (the installed version was used for the build).
- ESP32 Arduino core: **3.3.4** was used for the build.
- Serial Monitor: **115200 baud**. Choose the board's COM port to upload.

The CYD display adapter uses the ESP32 core's SPI library directly. No
TFT_eSPI User_Setup changes or additional display libraries are required.
The legacy TTGO adapter is compiled only if `BLEHUD_ENABLE_TTGO` is defined.

## Display wiring (already connected on the CYD)

| Signal | GPIO |
| --- | --- |
| SCLK | 14 |
| MOSI | 13 |
| MISO | 12 |
| CS | 15 |
| DC | 2 |
| Backlight, active HIGH | 21 |
| Reset | Board reset (software reset used) |

Physical output is **320 x 240 landscape**, RGB565, HSPI at 27 MHz.
A **160 x 120** canvas is doubled in both axes. It uses 38,400 bytes instead
of a 153,600-byte full-screen framebuffer, leaving more RAM for BLE on this
board without PSRAM. Direction and speed-limit images are 128 x 128 on the
physical panel. The startup logo and icon pair are centered horizontally.

In `TFT_CYD.cpp`, change `LANDSCAPE` from `0x28` to `0xE8` to rotate the
landscape view by 180 degrees. Keep `DISPLAY_ROTATION` at `0` in the sketch;
the legacy software 90/270-degree transforms are not suitable for this
rectangular canvas. `DISPLAY_MIRRORED` can be set to `1` for HUD reflection.

## Use

The BLE name is **ESP32 HUD**. The service UUIDs and navigation packet format
are unchanged. Connect with the original navigation application.
Hold **BOOT** for at least 500 ms, then release to enter deep sleep.
Press BOOT again to wake. The TTGO battery measurement and its second button
are removed because the battery-enable pin conflicts with CYD display SCLK.
Touch input is not used by this navigation sketch.

After upload, check the startup logo, BLE connection, turn arrow, speed-limit
colors, navigation text and BOOT sleep/wake on the actual board.

## References

- Board pinout: https://esp3d.io/esp3d-tft/version_1x/hardware/esp32/sunton-28-2432/
- ILI9341 alternative initialization register values: TFT_eSPI
  `TFT_Drivers/ILI9341_Init.h` (`ILI9341_2_DRIVER`).
