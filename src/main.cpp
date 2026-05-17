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
#define FIRMWARE_VERSION "1.6.1r19"
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
double filteredTemp = 0.0;
float  tempFilterAlpha = 0.4f; // EMA étage 2 (après MA) — 0.4 recommandé avec TEMP_MA_WINDOW=7

// Buffer filtre MA (moyenne glissante étage 1)
static double maBuffer[TEMP_MA_WINDOW] = {};
static double maSum    = 0.0;
static int    maIdx    = 0;
static bool   maFilled = false;

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
unsigned long idleGraphUpdateInterval = 1000;
float tempSamples[GRAPH_W];
int   tempSampleIdx     = 0;
bool  tempSamplesFilled = false;

// PID reset (v1.6.1+)
bool disablePidReset = false;

// Programme courant
String         currentProgramName = "Idle";
int            programStep        = 0;
unsigned long  programStartMs     = 0;
double         programStartTemp   = 0.0; // Température au démarrage du programme (pour block view multi-device)
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

// Boost initial
double boostStartTemp = 0.0;

// PID controller — utilise filteredTemp pour atténuer le bruit de mesure
PID tempPID(&filteredTemp, &pidOutput, &targetTemp, pidKp, pidKi, pidKd, DIRECT);

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

        // Filtre étage 1 : moyenne glissante MA(TEMP_MA_WINDOW) avec somme courante O(1)
        maSum -= maBuffer[maIdx];
        maBuffer[maIdx] = currentTemp;
        maSum += currentTemp;
        maIdx = (maIdx + 1) % TEMP_MA_WINDOW;
        if (maIdx == 0) maFilled = true;
        int    maCount = maFilled ? TEMP_MA_WINDOW : (maIdx == 0 ? TEMP_MA_WINDOW : maIdx);
        double maTemp  = maSum / maCount;

        // Filtre étage 2 : EMA légère sur la moyenne (α = tempFilterAlpha)
        filteredTemp = (filteredTemp == 0.0)
            ? maTemp
            : (tempFilterAlpha * maTemp + (1.0 - tempFilterAlpha) * filteredTemp);

        lastTempRead = millis();

        // Sécurité température
        // NaN + MIN sur valeur brute : détection immédiate de défaut capteur
        // MAX sur valeur filtrée : résistant aux pics de bruit near the limit
        if (isnan(currentTemp) || currentTemp < MIN_TEMP || filteredTemp > MAX_TEMP)
        {
            DEBUG_PRINTF("Temperature safety shutoff: raw=%.1f filtered=%.1f\n",
                         currentTemp, filteredTemp);
            RelaySerial::setRelay(1, false);
            RelaySerial::setRelay(2, false);
            return;
        }
    }

    // --- Calcul PID ---
    if (millis() - lastPIDCompute >= PID_INTERVAL)
    {
        static double pidLastInput = 0.0;

        if (currentPhase == PHASE_BOOST)
        {
            // Bypass PID : pleine puissance pendant la phase de boost
            pidOutput = 1023;
            pidPterm  = 1023; pidIterm = 0; pidDterm = 0;
            bool     ssr1On    = programRunning;
            uint16_t ssr2Value = programRunning ? 1023 : 0;
            if (ssr1On != lastSsr1State)
            { lastSsr1State = ssr1On; RelaySerial::setRelay(1, ssr1On); }
            if (ssr2Value != lastSsr2Pwm)
            { lastSsr2Pwm = ssr2Value; RelaySerial::setRelayPWM(2, ssr2Value); }
            ssr2Pwm = ssr2Value;
        }
        else if (tempPID.Compute())
        {
            // Décomposition des termes PID (la lib ne les expose pas)
            // P = Kp × error,  D = −Kd × ΔInput,  I = sortie − P − D
            // Le PID travaille sur filteredTemp pour atténuer le bruit de mesure.
            double err    = targetTemp - filteredTemp;
            double dInput = filteredTemp - pidLastInput;
            pidPterm = (float)(pidKp * err);
            pidDterm = (float)(-pidKd * dInput);
            pidIterm = (float)(pidOutput - pidPterm - pidDterm);

            uint16_t pwm       = (uint16_t)constrain((int)round(pidOutput), 0, 1023);
            bool     ssr1On    = programRunning;
            uint16_t ssr2Value = programRunning ? pwm : 0;

            if (ssr1On != lastSsr1State)
            { lastSsr1State = ssr1On; RelaySerial::setRelay(1, ssr1On); }
            if (ssr2Value != lastSsr2Pwm)
            { lastSsr2Pwm = ssr2Value; RelaySerial::setRelayPWM(2, ssr2Value); }

            ssr2Pwm = ssr2Value;
        }
        pidLastInput   = filteredTemp;
        lastPIDCompute = millis();
    }

    // --- Buffer graphe circulaire (cadence = idleGraphUpdateInterval) ---
    // La fenêtre affichée = GRAPH_W × idleGraphUpdateInterval
    // (ex : 300 × 1 s = 5 min, 300 × 5 s = 25 min)
    static unsigned long lastGraphSample = 0;
    if (millis() - lastGraphSample >= idleGraphUpdateInterval)
    {
        tempSamples[tempSampleIdx++] = (float)filteredTemp;
        if (tempSampleIdx >= GRAPH_W) { tempSampleIdx = 0; tempSamplesFilled = true; }
        lastGraphSample = millis();
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
