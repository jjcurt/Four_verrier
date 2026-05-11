#pragma once

// =============================================================================
// program_executor.h — Exécution des programmes de cuisson
//
// Extrait de main.cpp :
//   - loadProgramFromSD(name) → charge un programme JSON depuis la SD
//   - executeProgramStep()    → machine à états RAMP/HOLD, appelée dans loop()
//   - startDataLog(name)      → déjà dans data_logger.h, référencé ici pour clarté
//
// Ce module est indépendant du TFT et du serveur web.
// Il lit/modifie les variables globales de state suivantes :
//   currentProgram, currentPhase, programRunning, programLoaded,
//   phaseStartMs, programStartMs, rampStartTemp, targetTemp,
//   isStabilizing, stabilizingStartMs, initialBoost
// =============================================================================

#include <Arduino.h>
#include "furnace_types.h"

// Charge un programme de cuisson depuis /programs/<name>.json sur la SD card.
// Remplit la structure globale `currentProgram`.
// Retourne true en cas de succès.
bool loadProgramFromSD(const String &name);

// Exécute la logique de progression du programme courant (ramp → hold → next step).
// À appeler à chaque itération de loop().
// Ne fait rien si programRunning == false.
void executeProgramStep();
