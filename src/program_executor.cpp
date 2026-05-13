// =============================================================================
// program_executor.cpp — Exécution des programmes de cuisson
// =============================================================================

#include "program_executor.h"
#include "furnace_types.h"
#include "data_logger.h"
#include "settings_manager.h"
#include "debug.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <PID_v1.h>

// ---------------------------------------------------------------------------
// Variables globales de main.cpp
// ---------------------------------------------------------------------------
extern FiringProgram  currentProgram;
extern ProgramPhase   currentPhase;
extern bool           programRunning;
extern bool           programLoaded;
extern String         currentProgramName;
extern double         currentTemp;
extern double         targetTemp;
extern unsigned long  programStartMs;
extern unsigned long  phaseStartMs;
extern unsigned long  stepStartMs;
extern unsigned long  effectiveHoldStartMs;
extern float          rampStartTemp;
extern double         boostStartTemp;
extern double         pidOutput;
extern bool           isStabilizing;
extern unsigned long  stabilizingStartMs;
extern bool           disablePidReset;
extern double         stabilizingToleranceCritical;
extern double         stabilizingToleranceWarning;
extern PID            tempPID;

// ---------------------------------------------------------------------------
// loadProgramFromSD — Charge un programme JSON depuis /programs/<name>.json
// ---------------------------------------------------------------------------
bool loadProgramFromSD(const String &programName)
{
    String path = "/programs/" + programName;
    if (!path.endsWith(".json"))
        path += ".json";

    if (!SD.exists(path))
    {
        DEBUG_PRINTF("Program file not found: %s\n", path.c_str());
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        DEBUG_PRINTF("Failed to open program file: %s\n", path.c_str());
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        DEBUG_PRINTF("Failed to parse program JSON: %s\n", error.c_str());
        return false;
    }

    const char *name = doc["name"] | "Unknown";
    strncpy(currentProgram.name, name, sizeof(currentProgram.name) - 1);
    currentProgram.name[sizeof(currentProgram.name) - 1] = '\0';

    currentProgram.numSteps = doc["numSteps"] | 0;
    if (currentProgram.numSteps == 0 || currentProgram.numSteps > 10)
    {
        DEBUG_PRINTLN("Invalid number of steps");
        return false;
    }

    JsonArray stepsArray = doc["steps"];
    if (!stepsArray || stepsArray.size() != currentProgram.numSteps)
    {
        DEBUG_PRINTLN("Steps array missing or size mismatch");
        return false;
    }

    for (uint8_t i = 0; i < currentProgram.numSteps; i++)
    {
        JsonObject s = stepsArray[i];
        currentProgram.steps[i].targetTemp = s["targetTemp"] | 0.0f;
        currentProgram.steps[i].rampRate   = s["rampRate"]   | 0.0f;
        currentProgram.steps[i].holdTime   = s["holdTime"]   | 0;
        currentProgram.steps[i].withRamp   = s["withRamp"]   | false;
    }

    currentProgram.enableDataLog        = doc["enableDataLog"]        | false;
    currentProgram.enableTestLog        = doc["enableTestLog"]        | false;
    currentProgram.enableMaintenanceLog = doc["enableMaintenanceLog"] | false;
    currentProgram.dataLogInterval      = doc["dataLogInterval"]      | 5000;
    currentProgram.adaptiveLogging      = doc["adaptiveLogging"]      | false;
    currentProgram.adaptiveTempDelta    = doc["adaptiveTempDelta"]    | 0.5f;
    currentProgram.adaptivePwmDelta     = doc["adaptivePwmDelta"]     | 5.0f;
    currentProgram.manualMode           = doc["manualMode"]           | false;
    currentProgram.manualPWM            = doc["manualPWM"]            | 0;
    currentProgram.disablePidReset      = doc["disablePidReset"]      | disablePidReset;
    currentProgram.pidKp                = doc["pidKp"]                | 0.0f;
    currentProgram.pidKi                = doc["pidKi"]                | 0.0f;
    currentProgram.pidKd                = doc["pidKd"]                | 0.0f;
    currentProgram.enableBoost          = doc["enableBoost"]          | false;
    currentProgram.boostTempRise        = doc["boostTempRise"]        | 5.0f;
    currentProgram.boostMaxSec          = doc["boostMaxSec"]          | (uint16_t)60;

    currentProgram.active      = false;
    currentProgram.currentStep = 0;

    DEBUG_PRINTF("Program loaded: %s (%d steps)\n",
                 currentProgram.name, currentProgram.numSteps);
    DEBUG_PRINTF("  Log: user=%s test=%s maint=%s adaptive=%s interval=%lums\n",
                 currentProgram.enableDataLog        ? "ON" : "OFF",
                 currentProgram.enableTestLog        ? "ON" : "OFF",
                 currentProgram.enableMaintenanceLog ? "ON" : "OFF",
                 currentProgram.adaptiveLogging      ? "ON" : "OFF",
                 currentProgram.dataLogInterval);
    return true;
}

// ---------------------------------------------------------------------------
// executeProgramStep — Machine à états RAMP → HOLD → étape suivante
// À appeler à chaque itération de loop().
// ---------------------------------------------------------------------------
void executeProgramStep()
{
    if (!programRunning || !programLoaded || currentProgram.numSteps == 0)
        return;

    unsigned long now       = millis();
    uint8_t       stepIndex = currentProgram.currentStep;

    // Programme terminé
    if (stepIndex >= currentProgram.numSteps)
    {
        programRunning     = false;
        programLoaded      = false;
        currentProgramName = "Idle";
        targetTemp         = 0;
        currentPhase       = PHASE_IDLE;
        restoreBasePid();
        stopDataLog();
        DEBUG_PRINTLN("Program completed");
        return;
    }

    ProgramStep &step = currentProgram.steps[stepIndex];

    switch (currentPhase)
    {
    case PHASE_BOOST:
    {
        double rise    = currentTemp - boostStartTemp;
        unsigned long elapsed = (now - phaseStartMs) / 1000UL;
        bool tempLifted  = (rise  >= currentProgram.boostTempRise);
        bool timedOut    = (elapsed >= currentProgram.boostMaxSec);

        if (tempLifted || timedOut)
        {
            DEBUG_PRINTF("Boost terminé : ΔT=+%.1f°C en %lus (%s)\n",
                         rise, elapsed, timedOut ? "timeout" : "décollage");
            // Reset PID propre : forcer pidOutput=0 avant SetMode(AUTOMATIC)
            // pour éviter que le bumpless transfer initialise l'intégrale à 1023
            // (valeur de boost), ce qui causerait un dépassement massif en rampe.
            tempPID.SetMode(MANUAL);
            pidOutput = 0.0;
            tempPID.SetMode(AUTOMATIC);
            currentPhase = PHASE_IDLE;
        }
        break;
    }

    case PHASE_IDLE:
        if (step.withRamp && step.rampRate > 0)
        {
            currentPhase  = PHASE_RAMP;
            phaseStartMs  = now;
            stepStartMs   = now;
            rampStartTemp = (float)currentTemp;
            DEBUG_PRINTF("Step %d: RAMP %.1f→%.1f°C @ %.1f°C/min\n",
                         stepIndex + 1, rampStartTemp, step.targetTemp, step.rampRate);
        }
        else
        {
            targetTemp           = step.targetTemp;
            currentPhase         = PHASE_HOLD;
            phaseStartMs         = now;
            stepStartMs          = now;
            effectiveHoldStartMs = now;
            DEBUG_PRINTF("Step %d: HOLD %.1f°C %dmin\n",
                         stepIndex + 1, step.targetTemp, step.holdTime);
        }
        break;

    case PHASE_RAMP:
    {
        float elapsedMin = (now - phaseStartMs) / 60000.0f;
        float newTarget  = rampStartTemp + step.rampRate * elapsedMin;

        bool rampDone = (step.targetTemp >= rampStartTemp)
                        ? (newTarget >= step.targetTemp)
                        : (newTarget <= step.targetTemp);

        if (rampDone)
        {
            targetTemp           = step.targetTemp;
            currentPhase         = PHASE_HOLD;
            phaseStartMs         = now;
            effectiveHoldStartMs = now;
            isStabilizing        = false;
            stabilizingStartMs   = 0;
            DEBUG_PRINTF("Step %d: RAMP done → HOLD %.1f°C %dmin\n",
                         stepIndex + 1, step.targetTemp, step.holdTime);
        }
        else
        {
            targetTemp = newTarget;
        }
        break;
    }

    case PHASE_HOLD:
    {
        // Attendre que la température soit proche de la consigne avant de démarrer
        // le décompte (utile pour refroidissement libre ou chauffe directe sans rampe)
        // Tolérance directionnelle : serrée si au-dessus de la consigne (refroidissement
        // ou dépassement), large si en-dessous (chauffe depuis le froid).
        float tempDelta = (float)(currentTemp - targetTemp);
        bool outOfRange = (tempDelta > 0.0f)
            ? (tempDelta > (float)stabilizingToleranceWarning)     // au-dessus : 5°C
            : ((-tempDelta) > (float)stabilizingToleranceCritical); // en-dessous : 50°C
        if (outOfRange)
        {
            phaseStartMs         = now;
            effectiveHoldStartMs = now;
            isStabilizing        = true;
            stabilizingStartMs   = now;
            break;
        }
        isStabilizing = false;

        unsigned long holdElapsedMs  = now - phaseStartMs;
        unsigned long holdDurationMs = step.holdTime * 60000UL;

        if (holdElapsedMs >= holdDurationMs)
        {
            currentProgram.currentStep++;
            currentPhase = PHASE_IDLE;
            DEBUG_PRINTF("Step %d: HOLD done\n", stepIndex + 1);

            if (!currentProgram.disablePidReset && !disablePidReset)
            {
                tempPID.SetMode(MANUAL);
                tempPID.SetMode(AUTOMATIC);
                DEBUG_PRINTLN("  PID integrator reset");
            }
        }
        break;
    }
    }
}
