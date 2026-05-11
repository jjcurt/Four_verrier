// =============================================================================
// User_Setup.h — Configuration TFT_eSPI pour ESP32-2432S028R (CYD)
// Restauré depuis l'historique git (v1.6.1)
// =============================================================================

#define USER_SETUP_INFO "User_Setup for ESP32-2432S028R (CYD)"

#define ILI9341_2_DRIVER

// Display configuration
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Color order
#define TFT_RGB 1

// Backlight control
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// Pin configuration
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TOUCH_CS 33

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_GFXFF
#define SMOOTH_FONT

// SPI frequencies
#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
#define USE_HSPI_PORT
