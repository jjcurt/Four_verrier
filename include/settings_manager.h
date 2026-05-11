#pragma once

// =============================================================================
// settings_manager.h — Chargement et sauvegarde des paramètres système
//
// Extrait de main.cpp :
//   - loadSettings()             → lit /config/settings.json depuis la SD
//   - saveSettings()             → écrit /config/settings.json sur la SD
//   - applySettingsFromJson(doc) → applique les valeurs d'un JsonDocument
//                                  aux variables globales (sans I/O SD)
//
// Utilisé par :
//   - setup() → loadSettings() au démarrage
//   - /save_settings HTTP handler → applySettingsFromJson() + saveSettings()
//   - handleWebSocketMessage() → saveSettings() si paramètres modifiés
// =============================================================================

#include <Arduino.h>
#include <ArduinoJson.h>

bool loadSettings();
bool saveSettings();

// Applique les valeurs d'un document JSON aux variables globales.
// N'écrit rien sur la SD — appeler saveSettings() ensuite si nécessaire.
void applySettingsFromJson(const JsonDocument &doc);
