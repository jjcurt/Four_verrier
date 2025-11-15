#define USER_SETUP_INFO "User_Setup for ESP32-2432S028R (CYD)"

#define ILI9341_2_DRIVER

// Display configuration
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
// #define TFT_INVERSION_ON

// Color order - use RGB for ESP32-2432S028R
#define TFT_RGB 1

// Backlight control
#define TFT_BL 21             // LED back-light control pin
#define TFT_BACKLIGHT_ON HIGH // Level to turn ON back-light

// Pin configuration
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15   // Chip select control pin
#define TFT_DC 2    // Data Command control pin
#define TFT_RST -1  // Reset pin (connected to ESP32 RST)
#define TOUCH_CS 33 // Touch screen CS pin

// Other options
#define LOAD_GLCD  // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH
#define LOAD_FONT6 // Font 6. Large 40 pixel font, needs ~2666 bytes in FLASH
#define LOAD_FONT7 // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH
#define LOAD_GFXFF // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48

#define SMOOTH_FONT

// Maximum SPI frequency for reading TFT
#define SPI_FREQUENCY 55000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
#define USE_HSPI_PORT