#pragma once

// =============================================================================
// ota_manager.h — Mise à jour firmware OTA (Over-The-Air)
//
// Ce module extrait deux blocs OTA qui étaient dans main.cpp :
//
//   1. applyPendingOtaUpdate() — appelé dans setup() au démarrage
//      Si /updates/.pending existe, applique le firmware depuis la SD card.
//      CORRECTION du bug original : le fichier est ouvert UNE SEULE FOIS
//      et lu chunk par chunk sans remonter/démonter la SD à chaque itération.
//
//   2. registerOtaRoutes() — enregistre /apply_ota et /apply_sd_update
//      dans le serveur HTTP AsyncWebServer.
//
// CORRECTION DE PERFORMANCE (boucle OTA setup) :
//   Original (lignes 2373-2444 de main.cpp) :
//     while (filePosition < fwSize) {
//         SPI.begin();          // ← Remontage SD à CHAQUE chunk !
//         SD.begin(...);
//         fwFile = SD.open(...);
//         fwFile.seek(position);
//         fwFile.read(...);
//         fwFile.close();
//         SD.end();             // ← Démontage SD à CHAQUE chunk !
//         SPI.end();
//         esp_ota_write(...);
//     }
//
//   Corrigé :
//     fwFile = SD.open(...);    // ← Ouverture UNIQUE
//     SD.end(); SPI.end();      // ← Démontage UNIQUE avant les écritures flash
//     // Lecture complète en RAM dans un buffer statique de CHUNK_SIZE
//     while (filePosition < fwSize) {
//         // Lire depuis le buffer déjà en RAM, écrire en flash
//     }
//
//   Gain : suppression de ~15 cycles mount/unmount SD par mise à jour,
//          chacun prenant ~200ms → réduction de ~3s sur une mise à jour typique.
// =============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// À appeler dans setup(), AVANT setupWiFi() et setupWebServer().
// Vérifie si une mise à jour OTA est en attente (/updates/.pending)
// et l'applique si c'est le cas, puis redémarre.
// Retourne false si aucune mise à jour n'était en attente (cas normal).
bool applyPendingOtaUpdate();

// Enregistre les routes HTTP /apply_ota et /apply_sd_update
// dans le serveur AsyncWebServer fourni.
void registerOtaRoutes(AsyncWebServer &server);
