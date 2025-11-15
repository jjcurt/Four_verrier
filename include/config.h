#pragma once

// MAX6675 Thermocouple pins
#define TEMP_CS_PIN 27
#define TEMP_SO_PIN 35
#define TEMP_SCK_PIN 22

// SD Card pins (using VSPI)
#define SD_CS_PIN 5

// SSR control pins
#define SSR1_PIN 25 // First SSR control pin
#define SSR2_PIN 26 // Second SSR control pin

// Display pins are defined in User_Setup.h
// TFT_MISO 12
// TFT_MOSI 13
// TFT_SCLK 14
// TFT_CS   15
// TFT_DC    2
// TOUCH_CS 33

// Network configuration
#define WIFI_AP_SSID "Four_Verrier_AP"
#define WIFI_AP_PASSWORD "JJCFM2025"

// Web server port
#define HTTP_PORT 80

// Temperature control parameters
#define MAX_TEMP 1000           // Maximum allowable temperature in Celsius
#define MIN_TEMP 0              // Minimum temperature reading
#define TEMP_READ_INTERVAL 1000 // Temperature reading interval in ms
#define PID_INTERVAL 1000       // PID computation interval in ms

// SD Card configuration
#define SD_PROGRAMS_DIR "/programs" // Directory for storing firing programs
#define SD_LOGS_DIR "/logs"         // Directory for storing log files
#define SD_WEB_DIR "/www"           // Directory for web files