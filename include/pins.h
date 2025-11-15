#pragma once

// Display and touch screen pins (already defined in User_Setup.h)
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS

// MAX6675 thermocouple pins (also shared with software serial for Nano)
// These pins are shared between MAX6675 and relay serial communication:
#define TEMP_CS_PIN 27  // GPIO27: Mode select (HIGH=MAX6675, LOW=Serial)
#define TEMP_SO_PIN 35  // GPIO35: MAX6675 SO when CS=HIGH, Serial RX when CS=LOW
#define TEMP_SCK_PIN 22 // GPIO22: MAX6675 SCK when CS=HIGH, Serial TX when CS=LOW

// SD Card pin
#define SD_CS_PIN 5 // CS pin for SD card

// We must use the external Arduino Nano as relay driver due to limited GPIO
#define USE_RELAY_SERIAL 1

// Display dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE_SMALL 2
#define FONT_SIZE_LARGE 4

// Temperature control parameters
#define MAX_TEMP 1000           // Maximum allowable temperature in Celsius
#define MIN_TEMP 0              // Minimum temperature reading
#define TEMP_READ_INTERVAL 1000 // Temperature reading interval in ms
#define PID_INTERVAL 1000       // PID computation interval in ms

// WiFi settings

#define HTTP_PORT 80
// Optional: credentials for your local WiFi network (station mode)
// If both are non-empty, the ESP will try to connect as a station first
#define WIFI_STA_SSID ""
#define WIFI_STA_PASSWORD ""