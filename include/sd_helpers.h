#pragma once

// =============================================================================
// sd_helpers.h — Utilitaires SD card partagés par le serveur web
//
// RAISON D'ÊTRE :
//   Dans la version originale, le code de listage d'un répertoire SD était
//   dupliqué à l'identique dans 4 handlers HTTP :
//     - /programs      (lignes 1459-1476)
//     - /api/programs  (lignes 1478-1496)
//     - /api/logs      (lignes 1498-1515)
//     - /api/list      (paramétrique, lignes 1424-1446)
//
//   Cette duplication est remplacée par une seule fonction réutilisable.
//
// UTILISATION dans setupWebServer() :
//
//   #include "sd_helpers.h"
//
//   server.on("/programs", HTTP_GET, [](AsyncWebServerRequest *r) {
//       r->send(200, "application/json", sdListDirectoryJson("/programs"));
//   });
//   server.on("/api/programs", HTTP_GET, [](AsyncWebServerRequest *r) {
//       r->send(200, "application/json", sdListDirectoryJson("/programs"));
//   });
//   server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *r) {
//       r->send(200, "application/json", sdListDirectoryJson("/logs"));
//   });
//   server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *r) {
//       String dir = r->hasParam("dir") ? r->getParam("dir")->value() : "/";
//       r->send(200, "application/json", sdListDirectoryJson(dir));
//   });
//
// VALIDATION DU CHEMIN (sécurité) :
//   La fonction de validation sdIsAllowedPath() empêche l'accès à des
//   répertoires sensibles via des requêtes malformées (path traversal).
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "config.h"

// Liste les noms de fichiers d'un répertoire SD et retourne un tableau JSON.
// Retourne "[]" si le répertoire n'existe pas ou est vide.
// Le paramètre maxFiles limite la taille du document JSON (défaut : 64 fichiers).
inline String sdListDirectoryJson(const String &dir, size_t maxFiles = 64)
{
    // Capacité : 64 entrées x ~32 chars = ~2KB, DynamicJsonDocument pour flexibilité
    DynamicJsonDocument doc(64 * 48);
    JsonArray arr = doc.to<JsonArray>();

    File root = SD.open(dir);
    if (root && root.isDirectory())
    {
        size_t count = 0;
        File f;
        while ((f = root.openNextFile()) && count < maxFiles)
        {
            String name = f.name();
            // Supprimer le chemin du répertoire parent (SD.h ajoute le chemin complet)
            int lastSlash = name.lastIndexOf('/');
            if (lastSlash >= 0)
                name = name.substring(lastSlash + 1);
            // Ignorer les fichiers cachés (ex: .pending, .DS_Store)
            if (name.length() > 0 && name[0] != '.')
            {
                arr.add(name);
                count++;
            }
            f.close();
        }
        root.close();
    }

    String out;
    serializeJson(arr, out);
    return out;
}

// Vérifie qu'un chemin demandé par un client HTTP est dans une zone autorisée.
// À utiliser dans /download et /api/delete pour éviter le path traversal.
//
// Retourne true si le chemin est autorisé, false sinon.
inline bool sdIsAllowedPath(const String &path)
{
    // Refuser les chemins contenant des séquences de traversal
    if (path.indexOf("..") >= 0)
        return false;

    // Seuls ces répertoires sont accessibles depuis le web
    return path.startsWith(SD_LOGS_DIR)     ||
           path.startsWith(SD_PROGRAMS_DIR) ||
           path.startsWith(SD_UPDATES_DIR)  ||
           path.startsWith(SD_CONFIG_DIR);
}

// Vérifie l'existence d'un fichier sur la SD card.
// Wrapper simple pour lisibilité dans les handlers HTTP.
inline bool sdFileExists(const String &path)
{
    return SD.exists(path);
}
