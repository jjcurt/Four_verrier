#pragma once

// =============================================================================
// data_logger.h — Enregistrement des données de cuisson (CSV sur SD)
//
// Extrait de main.cpp :
//   - startDataLog(programName) → ouvre le fichier CSV horodaté
//   - stopDataLog()             → ferme et finalise le fichier
//   - logDataPoint()            → écrit une ligne dans le CSV
//
// Variables gérées ici (retirées du scope global de main.cpp) :
//   dataLogFile, dataLogActive, lastDataLogMs, currentLogType,
//   adaptiveLoggingEnabled, lastLoggedTemp, lastLoggedPwm, lastLogReason
// =============================================================================

#include <Arduino.h>

// Ouvre un nouveau fichier de log CSV pour le programme donné.
// Le type de log (USER/TEST/MAINTENANCE) est déduit de currentProgram.
void startDataLog(const String &programName);

// Ferme le fichier de log en cours.
void stopDataLog();

// Écrit un point de données dans le fichier de log actif.
// Tient compte du mode adaptatif (ne log que si delta temp ou PWM suffisant).
void logDataPoint();

// Retourne true si un log est en cours d'enregistrement
bool isDataLogActive();
