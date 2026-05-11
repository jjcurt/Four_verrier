#pragma once

// =============================================================================
// pins.h — Broches matérielles de l'ESP32
//
// CHANGEMENTS vs version précédente :
//   - Constantes dupliquées dans config.h supprimées (MAX_TEMP, MIN_TEMP,
//     TEMP_READ_INTERVAL, PID_INTERVAL, HTTP_PORT, SCREEN_WIDTH/HEIGHT)
//   - Credentials WiFi déplacés dans secrets.h
// =============================================================================

// --- Écran tactile XPT2046 ---
#define XPT2046_IRQ  36  // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK  25  // T_CLK
#define XPT2046_CS   33  // T_CS

// --- Thermocouple MAX6675 (bus SPI partagé avec RelaySerial) ---
// GPIO27 : sélecteur de mode (HIGH=MAX6675, LOW=Serial)
// GPIO35 : SO (MAX6675) / RX (Serial)
// GPIO22 : SCK (MAX6675) / TX (Serial)
#define TEMP_CS_PIN  27
#define TEMP_SO_PIN  35
#define TEMP_SCK_PIN 22

// --- Carte SD ---
#define SD_CS_PIN 5

// --- RelaySerial (Arduino Nano externe) ---
#define USE_RELAY_SERIAL 1
