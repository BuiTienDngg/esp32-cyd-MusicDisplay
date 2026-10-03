#pragma once
// Applied to all translation units, including the bundled TFT_eSPI library.
#define USER_SETUP_LOADED
#define ILI9341_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH
#define USE_HSPI_PORT
#define SPI_FREQUENCY 27000000
#define SPI_READ_FREQUENCY 16000000
#define LOAD_GLCD
#define LOAD_FONT2
#define SMOOTH_FONT
