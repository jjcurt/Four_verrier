#pragma once
#include <Arduino.h>

// Structure for a single program step
struct ProgramStep
{
    float targetTemp;  // Target temperature in Celsius
    float rampRate;    // Temperature increase rate (°C/minute, 0 for hold)
    uint32_t holdTime; // Hold time in minutes
    bool withRamp;     // Whether this step includes a ramp phase
};

// Structure for a complete firing program
struct FiringProgram
{
    char name[32];         // Program name
    uint8_t numSteps;      // Number of steps in the program
    ProgramStep steps[10]; // Array of program steps (max 10 steps)
    bool active;           // Whether this program is currently running
    uint8_t currentStep;   // Current step being executed
    bool enableDataLog;    // Whether to log temperature data to CSV during execution
};

// Structure for current furnace state
struct FurnaceState
{
    float currentTemp;      // Current temperature
    float targetTemp;       // Current target temperature
    float rampRate;         // Current ramp rate
    uint32_t stepStartTime; // Start time of current step
    uint32_t holdEndTime;   // End time of current hold
    bool ssr1Active;        // SSR 1 state
    bool ssr2Active;        // SSR 2 state (true if <> 0)
    uint8_t ssr2Pwm;        // SSR 2 PWM value
    bool programRunning;    // Whether a program is running
    char programName[32];   // Name of current program
    uint8_t currentStep;    // Current step number
    char statusMessage[64]; // Current status message
};

// PID control parameters structure
struct PIDParams
{
    double kp;        // Proportional gain
    double ki;        // Integral gain
    double kd;        // Derivative gain
    double outputMin; // Minimum output value
    double outputMax; // Maximum output value
};