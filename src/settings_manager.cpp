// =============================================================================
// settings_manager.cpp — Chargement et sauvegarde des paramètres système
// =============================================================================

#include "settings_manager.h"
#include "debug.h"
#include <SD.h>
#include <PID_v1.h>

// ---------------------------------------------------------------------------
// Variables globales de main.cpp
// ---------------------------------------------------------------------------
extern double         pidKp;
extern double         pidKi;
extern double         pidKd;
extern float          tempFilterAlpha;
extern unsigned long  tempReadInterval;

// Stabilisation (v1.4.7+)
extern double         stabilizingTimeoutMinutes;
extern double         stabilizingToleranceStrict;
extern double         stabilizingToleranceWarning;
extern double         stabilizingToleranceCritical;
extern unsigned long  stabilizingStableDurationMs;

// Estimation timeline (v1.4.9+)
extern double         estimatedHeatingRate;
extern double         estimatedCoolingRate;
extern double         estimatedStabilizationTime;

// HOLD (v1.4.7+)
extern double         cookingZoneMargin;

// Graphe (v1.5.11+)
extern unsigned long  idleGraphUpdateInterval;

// PID reset (v1.6.1+)
extern bool           disablePidReset;

// Contrôleur PID (pour SetTunings)
extern PID            tempPID;

static const char *SETTINGS_PATH = "/config/settings.json";

// ---------------------------------------------------------------------------
// applySettingsFromJson — Applique un document JSON aux variables globales
// (sans I/O SD — utile lors d'une mise à jour via WebSocket)
// ---------------------------------------------------------------------------
void applySettingsFromJson(const JsonDocument &doc)
{
    if (doc.containsKey("updateInterval"))
    {
        unsigned long v = doc["updateInterval"].as<unsigned long>();
        if (v >= 100 && v <= 60000) tempReadInterval = v;
    }
    if (doc.containsKey("pidKp")) pidKp = doc["pidKp"].as<double>();
    if (doc.containsKey("pidKi")) pidKi = doc["pidKi"].as<double>();
    if (doc.containsKey("pidKd")) pidKd = doc["pidKd"].as<double>();

    if (doc.containsKey("pidKp") || doc.containsKey("pidKi") || doc.containsKey("pidKd"))
        tempPID.SetTunings(pidKp, pidKi, pidKd);

    if (doc.containsKey("tempFilterAlpha"))    tempFilterAlpha    = doc["tempFilterAlpha"].as<float>();
    if (doc.containsKey("disablePidReset"))    disablePidReset    = doc["disablePidReset"].as<bool>();
    if (doc.containsKey("cookingZoneMargin"))  cookingZoneMargin  = doc["cookingZoneMargin"].as<double>();
    if (doc.containsKey("idleGraphUpdateInterval"))
        idleGraphUpdateInterval = doc["idleGraphUpdateInterval"].as<unsigned long>();

    if (doc.containsKey("stabilizingTimeoutMinutes"))
        stabilizingTimeoutMinutes    = doc["stabilizingTimeoutMinutes"].as<double>();
    if (doc.containsKey("stabilizingToleranceStrict"))
        stabilizingToleranceStrict   = doc["stabilizingToleranceStrict"].as<double>();
    if (doc.containsKey("stabilizingToleranceWarning"))
        stabilizingToleranceWarning  = doc["stabilizingToleranceWarning"].as<double>();
    if (doc.containsKey("stabilizingToleranceCritical"))
        stabilizingToleranceCritical = doc["stabilizingToleranceCritical"].as<double>();
    if (doc.containsKey("stabilizingStableDurationMs"))
        stabilizingStableDurationMs  = doc["stabilizingStableDurationMs"].as<unsigned long>();

    if (doc.containsKey("estimatedHeatingRate"))
        estimatedHeatingRate      = doc["estimatedHeatingRate"].as<double>();
    if (doc.containsKey("estimatedCoolingRate"))
        estimatedCoolingRate      = doc["estimatedCoolingRate"].as<double>();
    if (doc.containsKey("estimatedStabilizationTime"))
        estimatedStabilizationTime = doc["estimatedStabilizationTime"].as<double>();
}

// ---------------------------------------------------------------------------
// loadSettings — Lit /config/settings.json et applique les valeurs
// ---------------------------------------------------------------------------
bool loadSettings()
{
    if (!SD.exists(SETTINGS_PATH))
    {
        DEBUG_PRINTLN("No settings file found, using defaults");
        return false;
    }

    File file = SD.open(SETTINGS_PATH, FILE_READ);
    if (!file)
    {
        DEBUG_PRINTLN("Failed to open settings file");
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        DEBUG_PRINTF("Failed to parse settings: %s\n", error.c_str());
        return false;
    }

    // Utiliser les valeurs actuelles comme fallback
    tempReadInterval             = doc["updateInterval"]               | tempReadInterval;
    pidKp                        = doc["pidKp"]                        | pidKp;
    pidKi                        = doc["pidKi"]                        | pidKi;
    pidKd                        = doc["pidKd"]                        | pidKd;
    tempFilterAlpha              = doc["tempFilterAlpha"]              | tempFilterAlpha;
    stabilizingTimeoutMinutes    = doc["stabilizingTimeoutMinutes"]    | stabilizingTimeoutMinutes;
    stabilizingToleranceStrict   = doc["stabilizingToleranceStrict"]   | stabilizingToleranceStrict;
    stabilizingToleranceWarning  = doc["stabilizingToleranceWarning"]  | stabilizingToleranceWarning;
    stabilizingToleranceCritical = doc["stabilizingToleranceCritical"] | stabilizingToleranceCritical;
    stabilizingStableDurationMs  = doc["stabilizingStableDurationMs"]  | stabilizingStableDurationMs;
    estimatedHeatingRate         = doc["estimatedHeatingRate"]         | estimatedHeatingRate;
    estimatedCoolingRate         = doc["estimatedCoolingRate"]         | estimatedCoolingRate;
    estimatedStabilizationTime   = doc["estimatedStabilizationTime"]   | estimatedStabilizationTime;
    cookingZoneMargin            = doc["cookingZoneMargin"]            | cookingZoneMargin;
    idleGraphUpdateInterval      = doc["idleGraphUpdateInterval"]      | idleGraphUpdateInterval;
    disablePidReset              = doc["disablePidReset"]              | disablePidReset;

    tempPID.SetTunings(pidKp, pidKi, pidKd);

    DEBUG_PRINTF("Settings loaded: interval=%lu Kp=%.2f Ki=%.3f Kd=%.2f\n",
                 tempReadInterval, pidKp, pidKi, pidKd);
    return true;
}

// ---------------------------------------------------------------------------
// saveSettings — Écrit les paramètres courants dans /config/settings.json
// ---------------------------------------------------------------------------
bool saveSettings()
{
    StaticJsonDocument<512> doc;
    doc["updateInterval"]               = tempReadInterval;
    doc["pidKp"]                        = pidKp;
    doc["pidKi"]                        = pidKi;
    doc["pidKd"]                        = pidKd;
    doc["tempFilterAlpha"]              = tempFilterAlpha;
    doc["stabilizingTimeoutMinutes"]    = stabilizingTimeoutMinutes;
    doc["stabilizingToleranceStrict"]   = stabilizingToleranceStrict;
    doc["stabilizingToleranceWarning"]  = stabilizingToleranceWarning;
    doc["stabilizingToleranceCritical"] = stabilizingToleranceCritical;
    doc["stabilizingStableDurationMs"]  = stabilizingStableDurationMs;
    doc["estimatedHeatingRate"]         = estimatedHeatingRate;
    doc["estimatedCoolingRate"]         = estimatedCoolingRate;
    doc["estimatedStabilizationTime"]   = estimatedStabilizationTime;
    doc["cookingZoneMargin"]            = cookingZoneMargin;
    doc["idleGraphUpdateInterval"]      = idleGraphUpdateInterval;
    doc["disablePidReset"]              = disablePidReset;

    if (!SD.exists("/config")) SD.mkdir("/config");

    File file = SD.open(SETTINGS_PATH, FILE_WRITE);
    if (!file)
    {
        DEBUG_PRINTLN("Failed to create settings file");
        return false;
    }
    serializeJson(doc, file);
    file.close();

    DEBUG_PRINTLN("Settings saved");
    return true;
}
