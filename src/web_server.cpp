// =============================================================================
// web_server.cpp — Serveur HTTP et WebSocket
//
// CHANGEMENTS vs main.cpp original :
//
//  1. Routes SD dédupliquées : /programs, /api/programs, /api/logs, /api/list
//     appellent toutes sdListDirectoryJson() depuis sd_helpers.h
//     (suppression de ~60 lignes de code identique)
//
//  2. /upload et /upload_to unifiés via handleFileUploadBody()
//     (suppression de ~40 lignes dupliquées)
//
//  3. buildStatusJson() centralisé : utilisé par /status ET le broadcast
//     WebSocket dans loop() — plus de désynchronisation possible entre les deux
//
//  4. Validation de chemin ajoutée dans /download et /api/delete
//     via sdIsAllowedPath() pour prévenir le path traversal
//
//  5. /apply_ota déplacé dans ota_manager.cpp (voir ce fichier)
// =============================================================================

#include "web_server.h"
#include "debug.h"
#include "furnace_types.h"
#include "sd_helpers.h"
#include "settings_manager.h"
#include "program_executor.h"
#include "data_logger.h"
#include "config.h"
#include "secrets.h"
#include "wifi_manager.h"
#include <PID_v1.h>

// Variables globales de main.cpp référencées ici
extern double currentTemp;
extern double targetTemp;
extern double pidKp, pidKi, pidKd;
extern float  tempFilterAlpha;
extern unsigned long tempReadInterval;
extern uint16_t ssr2Pwm;
extern bool programRunning;
extern bool programLoaded;
extern bool initialBoost;
extern bool isStabilizing;
extern String currentProgramName;
extern FiringProgram currentProgram;
extern ProgramPhase currentPhase;
extern unsigned long programStartMs;
extern unsigned long phaseStartMs;
extern unsigned long stepStartMs;
extern unsigned long effectiveHoldStartMs;
extern double        boostStartTemp;
extern double        filteredTemp;
extern double        pidOutput;
extern PID tempPID;

// ---------------------------------------------------------------------------
// buildStatusJson — UNIQUE point de sérialisation du statut
// ---------------------------------------------------------------------------

String buildStatusJson()
{
    StaticJsonDocument<2048> doc;

    doc["temp"]           = currentTemp;
    doc["target"]         = targetTemp;
    doc["ssr1"]           = programRunning;
    doc["ssr2"]           = ssr2Pwm;
    doc["program"]        = programRunning ? 1 : 0;
    doc["programName"]    = currentProgramName;
    doc["programStep"]    = programLoaded ? (currentProgram.currentStep + 1) : 0;
    doc["totalSteps"]     = programLoaded ? currentProgram.numSteps : 0;
    doc["updateInterval"] = tempReadInterval;
    doc["pidKp"]          = pidKp;
    doc["pidKi"]          = pidKi;
    doc["pidKd"]          = pidKd;
    doc["tempFilterAlpha"]= tempFilterAlpha;
    doc["version"]        = FIRMWARE_VERSION;
    doc["emergency"]      = false;

    unsigned long now = millis();
    unsigned long elapsedMs = programRunning ? (now - programStartMs) : 0;
    doc["totalElapsedSeconds"] = elapsedMs / 1000;
    doc["elapsedSeconds"]      = elapsedMs / 1000; // alias pour compatibilité

    unsigned long stepElapsedMs = (programRunning && phaseStartMs > 0) ? (now - phaseStartMs) : 0;
    doc["stepElapsedSeconds"] = stepElapsedMs / 1000;

    unsigned long effHoldMs = (currentPhase == PHASE_HOLD && effectiveHoldStartMs > 0)
                              ? (now - effectiveHoldStartMs) : 0;
    doc["effectiveHoldMs"] = effHoldMs;

    const char *phaseStr = (isStabilizing && (currentPhase == PHASE_RAMP || currentPhase == PHASE_HOLD)) ? "STABILIZING"
                         : (currentPhase == PHASE_RAMP)  ? "RAMP"
                         : (currentPhase == PHASE_HOLD)  ? "HOLD"
                         : (currentPhase == PHASE_BOOST) ? "BOOST"
                         : "IDLE";
    doc["phase"] = phaseStr;

    // Ramp elapsed (count-up depuis stepStartMs) / Hold remaining (compte à rebours)
    unsigned long rampElapsedSec = 0;
    if (programRunning && programLoaded)
    {
        if (currentPhase == PHASE_RAMP)
            rampElapsedSec = (now - phaseStartMs) / 1000;
        else if (currentPhase == PHASE_HOLD && stepStartMs > 0 && phaseStartMs > stepStartMs)
            rampElapsedSec = (phaseStartMs - stepStartMs) / 1000;
    }
    doc["rampElapsedSeconds"] = rampElapsedSec;

    unsigned long holdRemainSec = 0;
    if (programRunning && programLoaded && currentProgram.numSteps > 0)
    {
        uint8_t si = currentProgram.currentStep;
        if (si >= currentProgram.numSteps) si = currentProgram.numSteps - 1;
        unsigned long holdTotal = (unsigned long)currentProgram.steps[si].holdTime * 60UL;
        if (currentPhase == PHASE_HOLD)
        {
            unsigned long holdElapsed = (now - phaseStartMs) / 1000;
            holdRemainSec = (holdElapsed < holdTotal) ? holdTotal - holdElapsed : 0;
        }
        else
        {
            holdRemainSec = holdTotal;
        }
    }
    doc["holdRemainingSeconds"] = holdRemainSec;

    doc["filteredTemp"] = filteredTemp;
    doc["pidOutput"]    = (uint16_t)constrain((int)round(pidOutput), 0, 1023);

    if (programLoaded && currentProgram.numSteps > 0)
    {
        JsonArray steps = doc.createNestedArray("programSteps");
        for (uint8_t i = 0; i < currentProgram.numSteps; i++)
        {
            JsonObject s = steps.createNestedObject();
            s["targetTemp"] = currentProgram.steps[i].targetTemp;
            s["rampRate"]   = currentProgram.steps[i].rampRate;
            s["holdTime"]   = currentProgram.steps[i].holdTime;
            s["withRamp"]   = currentProgram.steps[i].withRamp;
        }
    }

    String out;
    serializeJson(doc, out);
    return out;
}

// ---------------------------------------------------------------------------
// safeBroadcast
// ---------------------------------------------------------------------------

void safeBroadcast(const String &message)
{
    for (auto &client : ws.getClients())
    {
        if (client.queueLen() < 8)
            client.text(message);
        else
            DEBUG_PRINTF("[WS] Client #%u queue full (%d), skipping\n",
                         client.id(), client.queueLen());
    }
}

// ---------------------------------------------------------------------------
// WebSocket events
// ---------------------------------------------------------------------------

static void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT)
        return;

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data) != DeserializationError::Ok)
        return;

    bool settingsChanged = false;

    if (doc.containsKey("target"))         targetTemp = doc["target"].as<double>();
    if (doc.containsKey("updateInterval"))
    {
        unsigned long v = doc["updateInterval"].as<unsigned long>();
        if (v >= 100 && v <= 60000) { tempReadInterval = v; settingsChanged = true; }
    }
    if (doc.containsKey("pidKp")) { pidKp = doc["pidKp"].as<double>(); settingsChanged = true; }
    if (doc.containsKey("pidKi")) { pidKi = doc["pidKi"].as<double>(); settingsChanged = true; }
    if (doc.containsKey("pidKd")) { pidKd = doc["pidKd"].as<double>(); settingsChanged = true; }

    if (doc.containsKey("pidKp") || doc.containsKey("pidKi") || doc.containsKey("pidKd"))
        tempPID.SetTunings(pidKp, pidKi, pidKd);

    if (settingsChanged)
        saveSettings();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        DEBUG_PRINTF("WebSocket client #%u connected\n", client->id());
        break;
    case WS_EVT_DISCONNECT:
        DEBUG_PRINTF("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Handler d'upload unifié (/upload et /upload_to partagent ce corps)
// ---------------------------------------------------------------------------

static File s_uploadFile;
static String s_uploadPath;

static void handleFileUploadBody(AsyncWebServerRequest *request,
                                 const String &filename,
                                 size_t index, uint8_t *data, size_t len, bool final,
                                 const String &destFolder)
{
    if (index == 0)
    {
        String dest = destFolder;
        if (!dest.endsWith("/")) dest += "/";

        // Créer le répertoire de destination si nécessaire
        if (!SD.exists(dest))
        {
            DEBUG_PRINTF("Creating directory: %s\n", dest.c_str());
            SD.mkdir(dest);
        }

        s_uploadPath = dest + filename;
        DEBUG_PRINTF("Upload → %s\n", s_uploadPath.c_str());

        // Supprimer le fichier existant (FILE_WRITE appends sur ESP32)
        if (SD.exists(s_uploadPath))
            SD.remove(s_uploadPath);

        s_uploadFile = SD.open(s_uploadPath, FILE_WRITE);
        if (!s_uploadFile)
            DEBUG_PRINTF("Failed to open for write: %s\n", s_uploadPath.c_str());
    }

    if (s_uploadFile)
    {
        size_t written = s_uploadFile.write(data, len);
        if (written != len)
            DEBUG_PRINTF("Write error: %u/%u bytes\n", written, len);

        // Flush tous les 4 KB pour éviter de saturer la RAM
        if ((index + len) % 4096 == 0 || final)
            s_uploadFile.flush();

        if (final)
        {
            s_uploadFile.close();
            DEBUG_PRINTF("Upload complete: %s (%u bytes)\n", s_uploadPath.c_str(), index + len);

            // Si c'est un firmware → créer le marqueur .pending
            if (s_uploadPath.startsWith(SD_UPDATES_DIR) && s_uploadPath.endsWith(".bin"))
            {
                File marker = SD.open(String(SD_UPDATES_DIR) + "/.pending", FILE_WRITE);
                if (marker)
                {
                    marker.println("SD firmware update pending");
                    marker.close();
                    DEBUG_PRINTLN("Created .pending marker");
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// setupWebServer
// ---------------------------------------------------------------------------

void setupWebServer()
{
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // --- Fichiers statiques depuis la SD ---
    server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

    // Compatibilité : /files sert files.html
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *r) {
        if (SD.exists("/www/files.html"))
            r->send(SD, "/www/files.html", "text/html");
        else
            r->send(404, "text/plain", "files.html not found");
    });

    // -----------------------------------------------------------------------
    // API SD — listage de répertoires (UNIFIÉ via sdListDirectoryJson)
    // -----------------------------------------------------------------------

    server.on("/programs", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "application/json", sdListDirectoryJson(SD_PROGRAMS_DIR));
    });

    server.on("/api/programs", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "application/json", sdListDirectoryJson(SD_PROGRAMS_DIR));
    });

    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *r) {
        r->send(200, "application/json", sdListDirectoryJson(SD_LOGS_DIR));
    });

    // Route générique avec paramètre ?dir=
    server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *r) {
        String dir = r->hasParam("dir") ? r->getParam("dir")->value() : "/";
        r->send(200, "application/json", sdListDirectoryJson(dir));
    });

    // -----------------------------------------------------------------------
    // Téléchargement (avec validation de chemin)
    // -----------------------------------------------------------------------

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *r) {
        if (!r->hasParam("path")) { r->send(400, "text/plain", "missing path"); return; }
        String path = r->getParam("path")->value();

        // Sécurité : n'autoriser que les répertoires connus
        if (!sdIsAllowedPath(path)) { r->send(403, "text/plain", "forbidden"); return; }

        String filename = path.substring(path.lastIndexOf('/') + 1);
        AsyncWebServerResponse *resp = r->beginResponse(SD, path.c_str(), "application/octet-stream");
        if (resp)
        {
            resp->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
            r->send(resp);
        }
        else
        {
            r->send(404, "text/plain", "File not found");
        }
    });

    // -----------------------------------------------------------------------
    // Suppression (avec validation de chemin)
    // -----------------------------------------------------------------------

    server.on("/api/delete", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, (const char *)data, len))
            { r->send(400, "text/plain", "invalid json"); return; }

            const char *path = doc["path"] | "";
            if (strlen(path) == 0) { r->send(400, "text/plain", "missing path"); return; }

            // Sécurité : valider le chemin avant de supprimer
            if (!sdIsAllowedPath(String(path))) { r->send(403, "text/plain", "forbidden"); return; }

            SD.remove(String(path))
                ? r->send(200, "text/plain", "deleted")
                : r->send(500, "text/plain", "delete failed");
        });

    // -----------------------------------------------------------------------
    // Upload — /upload (destination /programs par défaut)
    // -----------------------------------------------------------------------

    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *r) { r->send(200, "text/plain", "Upload complete"); },
        [](AsyncWebServerRequest *r, const String &fn, size_t idx, uint8_t *data, size_t len, bool final)
        {
            // Destination : query param 'dest', sinon /programs
            String dest = r->hasParam("dest") ? r->getParam("dest")->value() : SD_PROGRAMS_DIR;
            handleFileUploadBody(r, fn, idx, data, len, final, dest);
        });

    // -----------------------------------------------------------------------
    // Upload — /upload_to (destination via query param ?dest=)
    // -----------------------------------------------------------------------

    server.on("/upload_to", HTTP_POST,
        [](AsyncWebServerRequest *r) { r->send(200, "text/plain", "Upload complete"); },
        [](AsyncWebServerRequest *r, const String &fn, size_t idx, uint8_t *data, size_t len, bool final)
        {
            String dest = r->hasParam("dest") ? r->getParam("dest")->value() : SD_PROGRAMS_DIR;
            handleFileUploadBody(r, fn, idx, data, len, final, dest);
        });

    // -----------------------------------------------------------------------
    // Sauvegarde d'un programme depuis l'éditeur web
    // -----------------------------------------------------------------------

    server.on("/api/save_program", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            DynamicJsonDocument doc(4096);
            if (deserializeJson(doc, (const char *)data, len))
            { r->send(400, "text/plain", "Invalid JSON"); return; }

            const char *name    = doc["name"]    | "";
            const char *content = doc["content"] | "";
            if (!strlen(name) || !strlen(content))
            { r->send(400, "text/plain", "Missing name or content"); return; }

            String filename = String(name);
            if (!filename.endsWith(".json")) filename += ".json";
            String path = String(SD_PROGRAMS_DIR) + "/" + filename;

            File f = SD.open(path, FILE_WRITE);
            if (!f) { r->send(500, "text/plain", "Failed to create file"); return; }
            f.print(content);
            f.close();

            DEBUG_PRINTF("Saved program: %s\n", path.c_str());
            r->send(200, "text/plain", "Program saved");
        });

    // -----------------------------------------------------------------------
    // OTA — délégué à ota_manager (voir src/ota_manager.cpp)
    // -----------------------------------------------------------------------
    extern void registerOtaRoutes(AsyncWebServer &server);
    registerOtaRoutes(server);

    // -----------------------------------------------------------------------
    // Contrôle du programme (start / stop)
    // -----------------------------------------------------------------------

    server.on("/start", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            StaticJsonDocument<128> doc;
            if (deserializeJson(doc, data, len)) { r->send(400, "text/plain", "Invalid JSON"); return; }
            String program = doc["program"] | "";
            if (program.length() == 0) { r->send(400, "text/plain", "No program specified"); return; }

            if (loadProgramFromSD(program))
            {
                programLoaded       = true;
                programRunning      = true;
                currentProgramName  = program;
                programStartMs      = millis();
                currentProgram.currentStep = 0;
                initialBoost        = ((currentProgram.steps[0].targetTemp - currentTemp) > 50.0f);

                if (currentProgram.enableBoost)
                {
                    currentPhase   = PHASE_BOOST;
                    phaseStartMs   = millis();
                    boostStartTemp = currentTemp;
                    DEBUG_PRINTF("[BOOST] Démarrage : T=%.1f°C, seuil=+%.1f°C, max=%us\n",
                                 currentTemp, currentProgram.boostTempRise, currentProgram.boostMaxSec);
                }
                else
                {
                    currentPhase = PHASE_IDLE;
                }

                // Override PID si le programme définit ses propres gains
                snapshotBasePid();
                if (currentProgram.pidKp != 0.0f || currentProgram.pidKi != 0.0f || currentProgram.pidKd != 0.0f)
                {
                    if (currentProgram.pidKp != 0.0f) pidKp = currentProgram.pidKp;
                    if (currentProgram.pidKi != 0.0f) pidKi = currentProgram.pidKi;
                    if (currentProgram.pidKd != 0.0f) pidKd = currentProgram.pidKd;
                    tempPID.SetTunings(pidKp, pidKi, pidKd);
                    DEBUG_PRINTF("[PID] Override programme: Kp=%.2f Ki=%.3f Kd=%.2f\n", pidKp, pidKi, pidKd);
                }

                if (currentProgram.enableDataLog || currentProgram.enableTestLog || currentProgram.enableMaintenanceLog)
                {
                    startDataLog(program);
                    logDataPoint();
                }
                DEBUG_PRINTF("Program started: %s\n", program.c_str());
                r->send(200, "text/plain", "Program started");
            }
            else
            {
                r->send(500, "text/plain", "Failed to load program file");
            }
        });

    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *r) {
        programRunning     = false;
        programLoaded      = false;
        currentProgramName = "Idle";
        targetTemp         = 0;
        currentPhase       = PHASE_IDLE;
        restoreBasePid();
        stopDataLog();
        DEBUG_PRINTLN("Program stopped");
        r->send(200, "text/plain", "Program stopped");
    });

    // -----------------------------------------------------------------------
    // Paramètres (lecture/écriture)
    // -----------------------------------------------------------------------

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *r) {
        AsyncWebServerResponse *resp = r->beginResponse(200, "application/json", buildStatusJson());
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        resp->addHeader("Pragma", "no-cache");
        resp->addHeader("Expires", "0");
        r->send(resp);
    });

    server.on("/save_settings", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        NULL,
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
        {
            StaticJsonDocument<512> doc;
            if (deserializeJson(doc, data, len)) { r->send(400, "text/plain", "Invalid JSON"); return; }
            applySettingsFromJson(doc);
            saveSettings()
                ? r->send(200, "text/plain", "Settings saved successfully")
                : r->send(500, "text/plain", "Failed to save settings to SD card");
        });

    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
        r->send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    });

    // -----------------------------------------------------------------------
    // Configuration WiFi
    // -----------------------------------------------------------------------

    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *r) {
        String sd_ssid, sd_password;
        String prefill = "";
        WifiStaticConfig _ignored;
        if (SD.exists("/config/wifi.json") && loadWiFiConfig(sd_ssid, sd_password, _ignored))
            prefill = sd_ssid;

        String html =
            "<html><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>WiFi Setup</title>"
            "<style>body{font-family:Arial;margin:12px;background:#f5f5f5}"
            ".card{background:white;padding:20px;border-radius:8px;margin-bottom:20px;"
            "box-shadow:0 2px 4px rgba(0,0,0,.1)}"
            "input{width:100%;padding:8px;margin:6px 0 12px;border:1px solid #ccc;"
            "border-radius:4px;box-sizing:border-box}"
            "button{background:#4CAF50;color:white;padding:10px 15px;border:none;"
            "border-radius:4px;cursor:pointer}</style></head><body>"
            "<div class='card'><h2>Paramètres WiFi</h2>"
            "<form method='POST' action='/save_wifi'>"
            "SSID:<br><input name='ssid' value='" + prefill + "'><br>"
            "Password:<br><input type='password' name='password'><br>"
            "<button type='submit'>Enregistrer &amp; Redémarrer</button>"
            "</form></div></body></html>";

        r->send(200, "text/html", html);
    });

    server.on("/save_wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
        const AsyncWebParameter *ssidP     = r->getParam("ssid",     true);
        const AsyncWebParameter *passP     = r->getParam("password", true);
        const AsyncWebParameter *staticIpP = r->getParam("staticIp", true);
        const AsyncWebParameter *gatewayP  = r->getParam("gateway",  true);
        const AsyncWebParameter *subnetP   = r->getParam("subnet",   true);
        const AsyncWebParameter *dnsP      = r->getParam("dns",      true);

        String ssid     = ssidP     ? ssidP->value()     : "";
        String pass     = passP     ? passP->value()     : "";
        String staticIp = staticIpP ? staticIpP->value() : "";
        String gateway  = gatewayP  ? gatewayP->value()  : "";
        String subnet   = subnetP   ? subnetP->value()   : "";
        String dns      = dnsP      ? dnsP->value()      : "";

        if (ssid.length() > 0 && saveWiFiConfig(ssid, pass, staticIp, gateway, subnet, dns))
        {
            // Répondre AVANT de redémarrer pour que le client reçoive bien le 200
            // (le redémarrage est déclenché par le client via /reboot)
            r->send(200, "text/plain", "OK");
        }
        else
        {
            r->send(500, "text/plain", "Failed to save WiFi configuration");
        }
    });

    server.begin();
}
