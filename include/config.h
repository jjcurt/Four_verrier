#pragma once

// =============================================================================
// config.h — Paramètres système du four verrier
//
// Ce fichier contient UNIQUEMENT des constantes non-sensibles.
// Les identifiants WiFi sont dans include/secrets.h (non versionné).
//
// CHANGEMENTS vs version précédente :
//   - WIFI_AP_SSID / WIFI_AP_PASSWORD déplacés dans secrets.h
//   - Doublons avec pins.h supprimés (MAX_TEMP, MIN_TEMP, TEMP_READ_INTERVAL,
//     PID_INTERVAL, HTTP_PORT) — ces constantes n'existent plus que dans config.h
// =============================================================================

// Firmware version (injectée par PlatformIO via build_flags si disponible)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.7.0r1"
#endif

// --- Serveur web ---
#define HTTP_PORT 80

// --- Paramètres de contrôle de température ---
#define MAX_TEMP 1000           // Température maximale autorisée (°C)
#define MIN_TEMP 0              // Température minimale de lecture (°C)
#define TEMP_READ_INTERVAL 1000 // Intervalle de lecture de la sonde (ms)
#define PID_INTERVAL 1000       // Intervalle de calcul PID (ms)

// Filtre température : moyenne glissante (étage 1) + EMA (étage 2)
// MA(7) réduit le bruit de quantification MAX31855 d'un facteur √7 ≈ 2.6×
// tempFilterAlpha (settings.json) contrôle l'EMA de lissage final (~0.4 recommandé)
#define TEMP_MA_WINDOW 7

// --- Répertoires SD card ---
#define SD_PROGRAMS_DIR "/programs"
#define SD_LOGS_DIR "/logs"
#define SD_WEB_DIR "/www"
#define SD_CONFIG_DIR "/config"
#define SD_UPDATES_DIR "/updates"

// --- Affichage ---
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE_SMALL 2
#define FONT_SIZE_LARGE 4

// --- Graphe température ---
#define GRAPH_W 300 // Nombre d'échantillons dans le buffer circulaire
#define GRAPH_H 60  // Hauteur en pixels (référence, zone affichage = 125px)

// --- Logging adaptatif ---
#define DEFAULT_LOG_INTERVAL 5000 // ms entre deux points de log
#define ADAPTIVE_TEMP_DELTA 0.5f  // °C minimum de variation pour logguer
#define ADAPTIVE_PWM_DELTA 51     // ~5% de 1023

//