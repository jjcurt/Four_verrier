#pragma once
#include <Arduino.h>

enum ProgramPhase { PHASE_IDLE, PHASE_BOOST, PHASE_RAMP, PHASE_HOLD };

struct ProgramStep
{
    float    targetTemp;
    float    rampRate;
    uint32_t holdTime;
    bool     withRamp;
};

struct FiringProgram
{
    char        name[32];
    uint8_t     numSteps;
    ProgramStep steps[10];
    bool        active;
    uint8_t     currentStep;

    bool     enableDataLog;
    bool     enableTestLog;
    bool     enableMaintenanceLog;
    uint32_t dataLogInterval;
    bool     adaptiveLogging;

    float adaptiveTempDelta;
    float adaptivePwmDelta;

    bool     manualMode;
    uint16_t manualPWM;

    bool disablePidReset;

    float pidKp;  // 0.0 = pas d'override (utilise settings.json)
    float pidKi;
    float pidKd;

    bool     enableBoost;    // pleine puissance avant de démarrer la rampe
    float    boostTempRise;  // °C de montée pour quitter le boost (défaut: 5.0)
    uint16_t boostMaxSec;    // durée max de sécurité en secondes (défaut: 60)
};

struct FurnaceState
{
    float    currentTemp;
    float    targetTemp;
    float    rampRate;
    uint32_t stepStartTime;
    uint32_t holdEndTime;
    bool     ssr1Active;
    bool     ssr2Active;
    uint8_t  ssr2Pwm;
    bool     programRunning;
    char     programName[32];
    uint8_t  currentStep;
    char     statusMessage[64];
};

struct PIDParams
{
    double kp;
    double ki;
    double kd;
    double outputMin;
    double outputMax;
};
