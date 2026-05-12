// =============================================================================
// data_logger.cpp — Enregistrement CSV sur SD des données de cuisson
// =============================================================================

#include "data_logger.h"
#include "furnace_types.h"
#include "debug.h"
#include "config.h"
#include <SD.h>
#include <time.h>

// ---------------------------------------------------------------------------
// État interne (non exposé dans le header)
// ---------------------------------------------------------------------------
enum LogType { LOG_NONE, LOG_USER, LOG_TEST, LOG_MAINTENANCE };

static File          dataLogFile;
static bool          dataLogActive         = false;
static unsigned long lastDataLogMs         = 0;
static LogType       currentLogType        = LOG_NONE;

// Adaptive logging
static bool          adaptiveLoggingEnabled = false;
static float         lastLoggedTemp         = 0.0f;
static uint16_t      lastLoggedPwm          = 0;
static float         adaptiveTempDelta      = 0.5f;   // °C
static float         adaptivePwmDelta       = 51.0f;  // ~5 % de 1023
static String        lastLogReason          = "INIT";

// ---------------------------------------------------------------------------
// Variables globales de main.cpp
// ---------------------------------------------------------------------------
extern FiringProgram  currentProgram;
extern double         currentTemp;
extern double         targetTemp;
extern double         rawTemp;
extern double         pidOutput;
extern double         filteredTemp;
extern float          pidError;
extern float          pidPterm;
extern float          pidIterm;
extern float          pidDterm;
extern double         pidKp;
extern double         pidKi;
extern double         pidKd;
extern uint16_t       ssr2Pwm;
extern bool           programLoaded;
extern unsigned long  programStartMs;
extern ProgramPhase   currentPhase;
extern unsigned long  phaseStartMs;
extern unsigned long  effectiveHoldStartMs;
extern bool           isStabilizing;
extern unsigned long  stabilizingStartMs;
extern bool           initialBoost;

// ---------------------------------------------------------------------------
// isDataLogActive
// ---------------------------------------------------------------------------
bool isDataLogActive()
{
    return dataLogActive;
}

// ---------------------------------------------------------------------------
// startDataLog — Ouvre le fichier CSV et écrit l'en-tête
// ---------------------------------------------------------------------------
void startDataLog(const String &programName)
{
    if (dataLogActive)
    {
        dataLogFile.close();
        dataLogActive = false;
    }

    if (currentProgram.enableMaintenanceLog)
        currentLogType = LOG_MAINTENANCE;
    else if (currentProgram.enableTestLog)
        currentLogType = LOG_TEST;
    else if (currentProgram.enableDataLog)
        currentLogType = LOG_USER;
    else
    {
        currentLogType = LOG_NONE;
        return;
    }

    // Nom de fichier horodaté
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);

    String safeName = programName;
    // Supprimer l'extension .json si présente (le nom du programme vient du nom de fichier)
    if (safeName.endsWith(".json") || safeName.endsWith(".JSON"))
        safeName.remove(safeName.length() - 5);
    safeName.replace(" ", "_");
    safeName.replace("/", "_");
    safeName.replace("\\", "_");

    const char *typePrefix = "";
    switch (currentLogType)
    {
    case LOG_USER:        typePrefix = "user_";  break;
    case LOG_TEST:        typePrefix = "test_";  break;
    case LOG_MAINTENANCE: typePrefix = "maint_"; break;
    default:              break;
    }

    String logPath = "/logs/" + String(typePrefix) + safeName + "_" + timestamp + ".csv";
    DEBUG_PRINTF("Starting log: %s\n", logPath.c_str());

    dataLogFile = SD.open(logPath, FILE_WRITE);
    if (!dataLogFile)
    {
        DEBUG_PRINTF("Failed to create log file: %s\n", logPath.c_str());
        currentLogType = LOG_NONE;
        return;
    }

    // En-tête CSV selon le type
    switch (currentLogType)
    {
    case LOG_USER:
        dataLogFile.println("Timestamp,ElapsedSeconds,CurrentTemp,TargetTemp,Phase,Step");
        break;
    case LOG_TEST:
        dataLogFile.println("Timestamp,ElapsedSeconds,CurrentTemp,RawTemp,TargetTemp,PidOutput,SSR2_PWM,Phase,Step");
        break;
    case LOG_MAINTENANCE:
        dataLogFile.println("Timestamp,ElapsedSeconds,CurrentTemp,FilteredTemp,RawTemp,TargetTemp,"
                            "Error,PidOutput,Pterm,Iterm,Dterm,PidKp,PidKi,PidKd,SSR2_PWM,"
                            "Phase,Step,RampRate,HoldTime,EffectiveHoldSec,StabilizingTimeSec,"
                            "InitialBoost,LogReason");
        break;
    default:
        dataLogFile.println("Timestamp,ElapsedSeconds,CurrentTemp,TargetTemp");
        break;
    }
    dataLogFile.flush();

    // Init adaptive logging
    if (currentProgram.adaptiveLogging)
    {
        adaptiveLoggingEnabled = true;
        lastLoggedTemp         = (float)currentTemp;
        lastLoggedPwm          = ssr2Pwm;
        adaptiveTempDelta      = currentProgram.adaptiveTempDelta;
        adaptivePwmDelta       = currentProgram.adaptivePwmDelta * 10.23f; // % → 0-1023
    }
    else
    {
        adaptiveLoggingEnabled = false;
    }

    dataLogActive = true;
    lastDataLogMs = millis();
    DEBUG_PRINTLN("Data logging started");
}

// ---------------------------------------------------------------------------
// stopDataLog — Ferme le fichier
// ---------------------------------------------------------------------------
void stopDataLog()
{
    if (dataLogActive)
    {
        dataLogFile.close();
        dataLogActive          = false;
        currentLogType         = LOG_NONE;
        adaptiveLoggingEnabled = false;
        DEBUG_PRINTLN("Data logging stopped");
    }
}

// ---------------------------------------------------------------------------
// logDataPoint — Écrit une ligne CSV (avec rate-limiting et adaptive sampling)
// ---------------------------------------------------------------------------
void logDataPoint()
{
    if (!dataLogActive || !dataLogFile)
        return;

    // Rate-limiting interne
    unsigned long logInterval = (currentProgram.dataLogInterval > 0)
                                ? currentProgram.dataLogInterval
                                : DEFAULT_LOG_INTERVAL;
    if (millis() - lastDataLogMs < logInterval)
        return;

    String logReason = "PERIODIC";

    // Adaptive sampling : ne logguer que si variation significative
    if (adaptiveLoggingEnabled)
    {
        float tempDelta = fabsf((float)currentTemp - lastLoggedTemp);
        float pwmDelta  = fabsf((float)ssr2Pwm    - (float)lastLoggedPwm);

        static uint8_t lastStep  = 255;
        static int     lastPhase = -1;
        bool stepOrPhaseChange   = (lastStep != currentProgram.currentStep)
                                 || (lastPhase != (int)currentPhase);

        if (stepOrPhaseChange)
        {
            lastStep  = currentProgram.currentStep;
            lastPhase = (int)currentPhase;
            logReason = "STEP_CHANGE";
        }
        else if (tempDelta >= adaptiveTempDelta)
        {
            logReason = "TEMP_DELTA";
        }
        else if (pwmDelta >= adaptivePwmDelta)
        {
            logReason = "PWM_DELTA";
        }
        else
        {
            return; // Pas assez de variation
        }

        lastLoggedTemp = (float)currentTemp;
        lastLoggedPwm  = ssr2Pwm;
    }
    lastLogReason = logReason;

    // Horodatage
    float elapsedSec = (millis() - programStartMs) / 1000.0f;
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    const char *phaseStr = (currentPhase == PHASE_RAMP) ? "RAMP"
                         : (currentPhase == PHASE_HOLD) ? "HOLD"
                                                        : "IDLE";
    int stepNum = programLoaded ? (currentProgram.currentStep + 1) : 0;

    switch (currentLogType)
    {
    case LOG_USER:
        dataLogFile.printf("%s,%.1f,%.2f,%.2f,%s,%d\n",
                           timestamp, elapsedSec,
                           currentTemp, targetTemp,
                           phaseStr, stepNum);
        break;

    case LOG_TEST:
        dataLogFile.printf("%s,%.1f,%.2f,%.2f,%.2f,%.2f,%u,%s,%d\n",
                           timestamp, elapsedSec,
                           currentTemp, rawTemp, targetTemp,
                           pidOutput, ssr2Pwm, phaseStr, stepNum);
        break;

    case LOG_MAINTENANCE:
    {
        float err = (float)(targetTemp - currentTemp);

        unsigned long effectiveHoldSec = (currentPhase == PHASE_HOLD && effectiveHoldStartMs > 0)
                                         ? (millis() - effectiveHoldStartMs) / 1000 : 0;
        unsigned long stabilizingTimeSec = (isStabilizing && stabilizingStartMs > 0)
                                           ? (millis() - stabilizingStartMs) / 1000 : 0;

        float rampRate  = programLoaded ? currentProgram.steps[currentProgram.currentStep].rampRate  : 0.0f;
        uint32_t holdTime = programLoaded ? currentProgram.steps[currentProgram.currentStep].holdTime : 0;

        dataLogFile.printf("%s,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"
                           ",%.2f,%.3f,%.3f,%u,%s,%d,%.2f,%lu,%lu,%lu,%d,%s\n",
                           timestamp, elapsedSec,
                           currentTemp, filteredTemp, rawTemp, targetTemp,
                           err, pidOutput, pidPterm, pidIterm, pidDterm,
                           pidKp, pidKi, pidKd, ssr2Pwm,
                           phaseStr, stepNum, rampRate, holdTime,
                           effectiveHoldSec, stabilizingTimeSec,
                           initialBoost ? 1 : 0, logReason.c_str());
        break;
    }

    default:
        dataLogFile.printf("%s,%.1f,%.2f,%.2f\n",
                           timestamp, elapsedSec, currentTemp, targetTemp);
        break;
    }

    dataLogFile.flush();
    lastDataLogMs = millis();
}
