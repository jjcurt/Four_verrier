// =============================================================================
// ota_manager.cpp — Mise à jour firmware OTA
// =============================================================================

#include "ota_manager.h"
#include "config.h"
#include "debug.h"
#include "sd_helpers.h"
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "TFT_eSPI.h"
#include <ArduinoJson.h>
#include "pins.h"

extern TFT_eSPI tft;

// ---------------------------------------------------------------------------
// Helpers internes
// ---------------------------------------------------------------------------

static void otaShowError(const String &msg)
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("ERREUR OTA:", 10, 10, 2);
    tft.drawString(msg, 10, 35, 2);
    DEBUG_PRINTF("OTA ERROR: %s\n", msg.c_str());
    delay(5000);
    ESP.restart();
}

static void otaShowProgress(size_t written, size_t total)
{
    int pct = (int)((float)written * 100.0f / (float)total);
    tft.fillRect(0, 60, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d%% (%u / %u Ko)", pct,
             (unsigned)(written / 1024), (unsigned)(total / 1024));
    tft.drawString(buf, 10, 60, 2);

    // Barre de progression
    int barW = (int)(280.0f * written / total);
    tft.fillRect(20, 85, 280, 12, TFT_DARKGREY);
    tft.fillRect(20, 85, barW, 12, TFT_GREEN);
}

// ---------------------------------------------------------------------------
// Moteur OTA commun : lit firmware depuis un File déjà ouvert, écrit en flash
//
// IMPORTANT : la SD DOIT être démontée (SD.end() / SPI.end()) AVANT d'appeler
// cette fonction. Les opérations esp_ota_write() et la SD partagent le bus SPI
// et ne peuvent pas coexister.
//
// Le fichier est entièrement lu en RAM chunk par chunk AVANT que la SD soit
// démontée. Cette version optimisée évite le cycle mount/unmount à chaque chunk.
// ---------------------------------------------------------------------------

static bool applyOtaFromFile(const String &firmwarePath, size_t fwSize,
                              bool showTftProgress = false)
{
    const size_t CHUNK_SIZE = 65536; // 64 KB — tient confortablement en RAM ESP32

    // 1. Allouer le buffer de chunk
    uint8_t *chunkBuffer = (uint8_t *)malloc(CHUNK_SIZE);
    if (!chunkBuffer)
    {
        DEBUG_PRINTLN("OTA: malloc failed");
        return false;
    }

    // 2. Obtenir la partition OTA cible
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        free(chunkBuffer);
        DEBUG_PRINTLN("OTA: no OTA partition");
        return false;
    }

    // 3. Commencer l'update OTA
    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, fwSize, &ota_handle);
    if (err != ESP_OK)
    {
        free(chunkBuffer);
        DEBUG_PRINTF("OTA: esp_ota_begin failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // 4. Ouvrir le fichier firmware
    File fwFile = SD.open(firmwarePath);
    if (!fwFile)
    {
        esp_ota_abort(ota_handle);
        free(chunkBuffer);
        DEBUG_PRINTF("OTA: cannot open %s\n", firmwarePath.c_str());
        return false;
    }

    // 5. Lire tout le firmware en RAM (chunk par chunk) AVANT de démonter la SD
    //    puis écrire immédiatement chaque chunk en flash.
    //
    //    Stratégie : lire un chunk → fermer SD → écrire flash → remonter SD → lire suivant
    //    Mais en pratique sur ESP32 avec OTA partition séparée du bus SPI SD,
    //    on peut lire et écrire sans démonter si les CS sont bien gérés.
    //    On conserve le démontage uniquement si SD_CS et SPI sont partagés avec flash.
    //
    //    Pour ce projet (SD sur VSPI, flash sur SPI interne), on peut tout faire
    //    sans démonter : lecture SD + écriture OTA en une seule passe.

    size_t filePosition = 0;
    bool success = true;
    String errorMsg;
    unsigned long lastYield = millis();

    while (filePosition < fwSize && success)
    {
        size_t bytesToRead = min(CHUNK_SIZE, fwSize - filePosition);

        // Lire le chunk depuis la SD
        size_t bytesRead = fwFile.read(chunkBuffer, bytesToRead);
        if (bytesRead != bytesToRead)
        {
            errorMsg = "Read error at offset " + String(filePosition);
            success = false;
            break;
        }

        // Écrire le chunk en flash OTA
        err = esp_ota_write(ota_handle, chunkBuffer, bytesRead);
        if (err != ESP_OK)
        {
            errorMsg = String("esp_ota_write: ") + esp_err_to_name(err);
            success = false;
            break;
        }

        filePosition += bytesRead;

        if (showTftProgress)
            otaShowProgress(filePosition, fwSize);

        // Yield watchdog toutes les 500ms
        if (millis() - lastYield > 500)
        {
            yield();
            lastYield = millis();
        }

        DEBUG_PRINTF("OTA progress: %u / %u (%.1f%%)\n",
                     (unsigned)filePosition, (unsigned)fwSize,
                     (float)filePosition * 100.0f / fwSize);
    }

    fwFile.close();
    free(chunkBuffer);

    if (!success)
    {
        esp_ota_abort(ota_handle);
        DEBUG_PRINTF("OTA aborted: %s\n", errorMsg.c_str());
        return false;
    }

    // 6. Finaliser
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        DEBUG_PRINTF("OTA: esp_ota_end failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // 7. Changer la partition de boot
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        DEBUG_PRINTF("OTA: set_boot_partition failed: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// applyPendingOtaUpdate — appelé dans setup()
// ---------------------------------------------------------------------------

bool applyPendingOtaUpdate()
{
    if (!SD.exists(String(SD_UPDATES_DIR) + "/.pending"))
        return false; // Rien à faire — cas normal

    DEBUG_PRINTLN("Pending OTA update detected");

    String firmwarePath = String(SD_UPDATES_DIR) + "/firmware.bin";
    if (!SD.exists(firmwarePath))
    {
        DEBUG_PRINTLN("ERROR: firmware.bin absent malgré .pending");
        SD.remove(String(SD_UPDATES_DIR) + "/.pending");

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("ERREUR: firmware absent", 10, 10, 2);
        delay(5000);
        ESP.restart();
        return true; // unreachable
    }

    File fwFile = SD.open(firmwarePath);
    size_t fwSize = fwFile.size();
    fwFile.close();

    DEBUG_PRINTF("Firmware size: %u bytes\n", (unsigned)fwSize);

    // Affichage TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Mise à jour firmware...", 10, 10, 2);
    tft.drawString("NE PAS COUPER L'ALIMENTATION", 10, 35, 1);

    bool ok = applyOtaFromFile(firmwarePath, fwSize, /*showTftProgress=*/true);

    if (ok)
    {
        // Nettoyage SD
        SD.remove(String(SD_UPDATES_DIR) + "/.pending");
        SD.remove(firmwarePath);

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("MISE A JOUR REUSSIE !", 10, 10, 2);
        tft.drawString("Redemarrage...", 10, 40, 2);
        delay(2000);
        ESP.restart();
    }
    else
    {
        otaShowError("OTA update failed — see serial log");
    }

    return true; // unreachable après restart
}

// ---------------------------------------------------------------------------
// Routes HTTP OTA
// ---------------------------------------------------------------------------

void registerOtaRoutes(AsyncWebServer &server)
{
    // POST /apply_ota { "path": "/updates/firmware.bin" }
    server.on("/apply_ota", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, (const char *)data, len))
            { r->send(400, "text/plain", "invalid json"); return; }

            const char *path = doc["path"] | "";
            if (!strlen(path)) { r->send(400, "text/plain", "missing path"); return; }

            if (!sdIsAllowedPath(String(path)))
            { r->send(403, "text/plain", "forbidden"); return; }

            File f = SD.open(String(path));
            if (!f) { r->send(404, "text/plain", "file not found"); return; }
            size_t fwSize = f.size();
            f.close();

            // Note : la réponse est envoyée AVANT le redémarrage
            r->send(200, "text/plain", "Update started, rebooting shortly");

            bool ok = applyOtaFromFile(String(path), fwSize, false);
            if (ok)
            {
                delay(500);
                ESP.restart();
            }
            // Si échec : l'ESP continue de fonctionner (pas de restart)
        });

    // POST /apply_sd_update { "version": "v1.2.3" }
    server.on("/apply_sd_update", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            String versionInfo = "Nouvelle version";
            if (len > 0)
            {
                StaticJsonDocument<256> doc;
                DeserializationError err = deserializeJson(doc, (const char *)data, len);
                if (!err && doc.containsKey("version"))
                    versionInfo = doc["version"].as<String>();
            }

            String firmwarePath = String(SD_UPDATES_DIR) + "/firmware.bin";
            if (!SD.exists(firmwarePath))
            { r->send(404, "text/plain", "firmware.bin not found in /updates/"); return; }

            File marker = SD.open(String(SD_UPDATES_DIR) + "/.pending", FILE_WRITE);
            if (!marker) { r->send(500, "text/plain", "Failed to create .pending marker"); return; }
            marker.println(versionInfo);
            marker.println("Created: " + String(millis() / 1000) + "s since boot");
            marker.close();

            r->send(200, "text/plain", "SD update scheduled - rebooting now");
            delay(1000);
            ESP.restart();
        });
}
