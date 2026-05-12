// =============================================================================
// main.cpp — Orchestrateur principal du Four Verrier (ESP32)
//
// APRÈS REFACTORISATION — ce fichier ne contient plus que :
//   1. Déclarations des objets globaux (hardware + état)
//   2. setup() : initialisation dans l'ordre correct
//   3. loop() : boucle principale allégée
//
// Toute la logique est déléguée aux modules :
//   - display_manager   → TFT (drawStaticUI, updateDisplay, displayStartupScreen)
//   - program_executor  → Machine à états programme (loadProgramFromSD, executeProgramStep)
//   - web_server        → HTTP + WebSocket (setupWebServer, buildStatusJson, safeBroadcast)
//   - ota_manager       → OTA SD et HTTP (applyPendingOtaUpdate, registerOtaRoutes)
//   - settings_manager  → Paramètres SD (loadSettings, saveSettings)
//   - data_logger       → CSV SD (startDataLog, stopDataLog, logDataPoint)
//   - sd_helpers        → Utilitaires SD partagés (sdListDirectoryJson, sdIsAllowedPath)
//
// Taille visée après refacto : ~200 lignes (vs 1870 dans la version originale)
// =============================================================================

// ---------------------------------------------------------------------------
// Debug logging (désactiver en production — conflit avec UART0/RelaySerial)
// ---------------------------------------------------------------------------
// #define DEBUG_SERIAL   // ← Décommenter uniquement en développement

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.6.1r9"
#endif
const char *firmwareVersion = FIRMWARE_VERSION;

#include "debug.h"

// ---------------------------------------------------------------------------
// Includes système
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <max6675.h>
#include <PID_v1.h>
#include "TFT_eSPI.h"
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ---------------------------------------------------------------------------
// Includes projet
// ---------------------------------------------------------------------------
#include "pins.h"           // Broches matérielles (MAX6675, SD, SSR...)
#include "config.h"         // Constantes système
#include "secrets.h"        // Credentials WiFi (NON versionné — voir secrets.h.example)
#include "furnace_types.h"  // Structures FiringProgram, FurnaceState, PIDParams
#include "display_manager.h"
#include "program_executor.h"
#include "web_server.h"
#include "ota_manager.h"
#include "settings_manager.h"
#include "data_logger.h"
#include "sd_helpers.h"
#include "wifi_manager.h"
#include <relay_serial.h>
#include <relay_pins.h>

// ---------------------------------------------------------------------------
// Objets hardware globaux
// ---------------------------------------------------------------------------
TFT_eSPI           tft          = TFT_eSPI(240, 320);
MAX6675            thermocouple(TEMP_SCK_PIN, TEMP_CS_PIN, TEMP_SO_PIN);
AsyncWebServer     server(HTTP_PORT);
AsyncWebSocket     ws("/ws");

// ---------------------------------------------------------------------------
// État global du four
// (Ces variables sont référencées via `extern` dans les modules)
// ---------------------------------------------------------------------------

// Température
double rawTemp      = 0.0;
double currentTemp  = 0.0;
double targetTemp   = 0.0;
double pidOutput    = 0.0;
float  filteredTemp = 0.0;
float  tempFilterAlpha = 0.8f;

// PID
double pidKp = 40.0, pidKi = 0.1, pidKd = 4.0;
float  pidError = 0.0, pidPterm = 0.0, pidIterm = 0.0, pidDterm = 0.0;

// SSR
uint16_t ssr2Pwm      = 0;
uint16_t lastSsr2Pwm  = 0;
bool     lastSsr1State = false;

// Intervalles
unsigned long tempReadInterval = TEMP_READ_INTERVAL;
unsigned long lastTempRead     = 0;
unsigned long lastPIDCompute   = 0;

// Paramètres de stabilisation (v1.4.7+)
double stabilizingTimeoutMinutes    = 20.0;
double stabilizingToleranceStrict   = 2.0;
double stabilizingToleranceWarning  = 5.0;
double stabilizingToleranceCritical = 50.0;
unsigned long stabilizingStableDurationMs = 30000;

// Paramètres d'estimation timeline (v1.4.9+)
double estimatedHeatingRate      = 39.0;
double estimatedCoolingRate      = 21.0;
double estimatedStabilizationTime = 3.0;

// Paramètres HOLD (v1.4.7+)
double cookingZoneMargin = 20.0;

// Graphe température
unsigned long idleGraphUpdateInterval = 5000;
float tempSamples[GRAPH_W];
int   tempSampleIdx     = 0;
bool  tempSamplesFilled = false;

// PID reset (v1.6.1+)
bool disablePidReset = false;

// Programme courant
String         currentProgramName = "Idle";
int            programStep        = 0;
unsigned long  programStartMs     = 0;
FiringProgram  currentProgram     = {};
bool           programLoaded      = false;
bool           programRunning     = false;

// Phase d'exécution
ProgramPhase  currentPhase  = PHASE_IDLE;  // type défini dans furnace_types.h
unsigned long phaseStartMs  = 0;
unsigned long stepStartMs   = 0;           // début de l'étape courante (ramp + hold)
float         rampStartTemp = 0;

// Stabilisation
bool          isStabilizing        = false;
unsigned long stabilizingStartMs   = 0;
unsigned long effectiveHoldStartMs = 0;
bool          initialBoost         = false;

// PID controller (doit être déclaré après les doubles qu'il référence)
PID tempPID(&currentTemp, &pidOutput, &targetTemp, pidKp, pidKi, pidKd, DIRECT);

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
#ifdef DEBUG_SERIAL
    Serial.begin(115200);
    delay(100);
    DEBUG_PRINTLN("\n=== Four Verrier ===");
    DEBUG_PRINTF("Firmware: %s\n", FIRMWARE_VERSION);
#endif

    // 1. TFT
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Demarrage...", 10, 10, 2);

    // 2. SD card
    if (!SD.begin(SD_CS_PIN, SPI))
    {
        tft.drawString("ERREUR: SD non trouvee", 10, 35, 2);
        DEBUG_PRINTLN("SD init failed");
        delay(5000);
        ESP.restart();
    }
    DEBUG_PRINTLN("SD OK");

    // 3. OTA en attente ? (avant tout le reste)
    applyPendingOtaUpdate(); // → redémarre si update appliqué

    // 4. PID
    tempPID.SetMode(AUTOMATIC);
    tempPID.SetOutputLimits(0, 1023);

    // 5. Paramètres depuis SD
    if (!loadSettings())
        DEBUG_PRINTLN("[SETUP] Pas de settings.json, valeurs par défaut");
    else
        DEBUG_PRINTF("[SETUP] Kp=%.2f Ki=%.3f Kd=%.2f EMA=%.2f\n",
                     pidKp, pidKi, pidKd, tempFilterAlpha);
    tempPID.SetTunings(pidKp, pidKi, pidKd);

    // 6. WiFi
    setupWiFi();

    // 7. Serveur web
    setupWebServer();

    // 8. NTP
    syncTimeWithNTP(8000);

    // 9. RelaySerial
    if (USE_RELAY_SERIAL)
    {
        RelaySerial::begin(RELAY_SERIAL_BAUD);
        RelaySerial::requestState(1);
        RelaySerial::requestState(2);
    }

    // 10. Affichage
    displayStartupScreen(); // IP, WiFi, NTP pendant 5 secondes
    drawStaticUI();         // Éléments statiques TFT

    DEBUG_PRINTLN("System ready!");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop()
{
    // --- Exécution programme (machine à états RAMP/HOLD) ---
    executeProgramStep();

    // --- Lecture température ---
    if (millis() - lastTempRead >= tempReadInterval)
    {
        rawTemp     = thermocouple.readCelsius();
        currentTemp = rawTemp;

        // Filtre EMA
        filteredTemp = (filteredTemp == 0.0f)
            ? (float)currentTemp
            : (tempFilterAlpha * currentTemp + (1.0f - tempFilterAlpha) * filteredTemp);

        lastTempRead = millis();

        // Buffer graphe circulaire
        tempSamples[tempSampleIdx++] = (float)currentTemp;
        if (tempSampleIdx >= GRAPH_W) { tempSampleIdx = 0; tempSamplesFilled = true; }

        // Sécurité température
        if (currentTemp < MIN_TEMP || currentTemp > MAX_TEMP)
        {
            DEBUG_PRINTLN("Temperature out of range — safety shutoff");
            RelaySerial::setRelay(1, false);
            RelaySerial::setRelay(2, false);
            return;
        }
    }

    // --- Calcul PID ---
    if (millis() - lastPIDCompute >= PID_INTERVAL)
    {
        if (tempPID.Compute())
        {
            uint16_t pwm       = (uint16_t)constrain((int)round(pidOutput), 0, 1023);
            bool     ssr1On    = programRunning;
            uint16_t ssr2Value = programRunning ? pwm : 0;

            if (ssr1On != lastSsr1State)
            { lastSsr1State = ssr1On; RelaySerial::setRelay(1, ssr1On); }
            if (ssr2Value != lastSsr2Pwm)
            { lastSsr2Pwm = ssr2Value; RelaySerial::setRelayPWM(2, ssr2Value); }

            ssr2Pwm = ssr2Value;
        }
        lastPIDCompute = millis();
    }

    // --- Data logging ---
    if (isDataLogActive() && programRunning)
        logDataPoint(); // logDataPoint() gère le rate-limiting via lastDataLogMs en interne

    // --- Affichage TFT ---
    updateDisplay(false);

    // --- WebSocket : nettoyage clients ---
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup >= 1000)
    { ws.cleanupClients(); lastCleanup = millis(); }

    // --- WebSocket : broadcast statut (2x/seconde) ---
    static unsigned long lastWebUpdate = 0;
    if (millis() - lastWebUpdate >= 500 && ws.count() > 0)
    {
        safeBroadcast(buildStatusJson()); // buildStatusJson() centralisé dans web_server.cpp
        lastWebUpdate = millis();
    }

    // --- RelaySerial I/O ---
    if (USE_RELAY_SERIAL) RelaySerial::loop();

    // --- Yield FreeRTOS (remplace delay(10) bloquant) ---
    vTaskDelay(pdMS_TO_TICKS(1));
}
