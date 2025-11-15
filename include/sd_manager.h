#pragma once
#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "furnace_types.h"
#include "config.h"

class SDManager
{
public:
    bool begin()
    {
        if (!SD.begin(SD_CS_PIN))
        {
            return false;
        }

        // Create required directories if they don't exist
        createDirIfNotExists(SD_PROGRAMS_DIR);
        createDirIfNotExists(SD_LOGS_DIR);
        createDirIfNotExists(SD_WEB_DIR);

        return true;
    }

    bool saveProgram(const FiringProgram &program)
    {
        String filename = String(SD_PROGRAMS_DIR) + "/" + program.name + ".json";
        File file = SD.open(filename, FILE_WRITE);
        if (!file)
        {
            return false;
        }

        StaticJsonDocument<1024> doc;
        doc["name"] = program.name;
        doc["numSteps"] = program.numSteps;

        JsonArray steps = doc.createNestedArray("steps");
        for (int i = 0; i < program.numSteps; i++)
        {
            JsonObject step = steps.createNestedObject();
            step["targetTemp"] = program.steps[i].targetTemp;
            step["rampRate"] = program.steps[i].rampRate;
            step["withRamp"] = program.steps[i].withRamp;
            step["holdTime"] = program.steps[i].holdTime;
            step["useSSR2"] = program.steps[i].useSSR2;
        }

        if (serializeJson(doc, file) == 0)
        {
            file.close();
            return false;
        }

        file.close();
        return true;
    }

    bool loadProgram(const char *name, FiringProgram &program)
    {
        String filename = String(SD_PROGRAMS_DIR) + "/" + name + ".json";
        File file = SD.open(filename);
        if (!file)
        {
            return false;
        }

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error)
        {
            return false;
        }

        strlcpy(program.name, doc["name"] | "", sizeof(program.name));
        program.numSteps = doc["numSteps"] | 0;
        program.active = false;
        program.currentStep = 0;

        JsonArray steps = doc["steps"];
        for (int i = 0; i < program.numSteps; i++)
        {
            program.steps[i].targetTemp = steps[i]["targetTemp"] | 0.0f;
            program.steps[i].rampRate = steps[i]["rampRate"] | 0.0f;
            program.steps[i].withRamp = steps[i]["withRamp"] | true;
            program.steps[i].holdTime = steps[i]["holdTime"] | 0;
            program.steps[i].useSSR2 = steps[i]["useSSR2"] | false;
        }

        return true;
    }

    void logData(const FurnaceState &state)
    {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to obtain time for logging");
            return;
        }
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d", &timeinfo);
        String filename = String(SD_LOGS_DIR) + "/log_" + timeStr + ".csv";

        File file = SD.open(filename, FILE_APPEND);
        if (!file)
        {
            return;
        }

        // Log format: timestamp,temp,target,ssr1,ssr2,program,step,status
        file.printf("%lu,%0.1f,%0.1f,%d,%d,%s,%d,%s\n",
                    millis(),
                    state.currentTemp,
                    state.targetTemp,
                    state.ssr1Active,
                    state.ssr2Active,
                    state.programName,
                    state.currentStep,
                    state.statusMessage);

        file.close();
    }

private:
    void createDirIfNotExists(const char *path)
    {
        if (!SD.exists(path))
        {
            SD.mkdir(path);
        }
    }
};