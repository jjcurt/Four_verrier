// =============================================================================
// display_manager.cpp — Gestion de l'affichage TFT (320×240)
// =============================================================================

#include "display_manager.h"
#include "display_layout.h"
#include "furnace_types.h"
#include "config.h"
#include "TFT_eSPI.h"
#include <WiFi.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Variables globales définies dans main.cpp
// ---------------------------------------------------------------------------
extern TFT_eSPI      tft;
extern double        currentTemp;
extern double        targetTemp;
extern bool          lastSsr1State;
extern uint16_t      ssr2Pwm;
extern bool          programLoaded;
extern bool          programRunning;
extern FiringProgram currentProgram;
extern unsigned long programStartMs;
extern ProgramPhase      currentPhase;
extern bool          isStabilizing;
extern unsigned long phaseStartMs;
extern float         tempSamples[];
extern int           tempSampleIdx;
extern bool          tempSamplesFilled;

// ---------------------------------------------------------------------------
// drawStaticUI — Cadre invariant (une seule fois au démarrage ou après reset)
// ---------------------------------------------------------------------------
void drawStaticUI()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);

    // Séparateurs horizontaux entre les trois zones
    tft.drawFastHLine(0, 57, SCREEN_WIDTH, TFT_DARKGREY);
    tft.drawFastHLine(0, DISP_GRAPH_TOP_Y, SCREEN_WIDTH, TFT_DARKGREY);

    // Labels SSR (valeur fixe, redessinée aussi par updateDisplay)
    tft.drawString("SSR1", DISP_SSR1_LABEL_X, DISP_SSR1_LABEL_Y, 2);
    tft.drawString("SSR2", DISP_SSR2_LABEL_X, DISP_SSR2_LABEL_Y, 2);

    // Cadre fixe de la barre PWM SSR2 (coordonnées identiques à updateDisplay)
    tft.drawRect(210, 40, 80, 14, TFT_WHITE);
}

// ---------------------------------------------------------------------------
// updateDisplay — Éléments dynamiques (appelé à chaque loop())
// ---------------------------------------------------------------------------
void updateDisplay(bool force)
{
    static unsigned long lastUpdate = 0;
    if (!force && millis() - lastUpdate < 1000)
        return;

    // -------------------------------------------------------------------------
    // PARTIE 1 — Bandeau supérieur (température, SSR, état programme)
    // -------------------------------------------------------------------------

    int tempColor = TFT_WHITE;
    if (currentTemp < targetTemp - 2.0)
        tempColor = TFT_CYAN;
    else if (currentTemp > targetTemp + 2.0)
        tempColor = TFT_ORANGE;
    else
        tempColor = TFT_GREEN;

    // Température four
    tft.fillRect(DISP_TEMP_X, DISP_TEMP_Y, DISP_TEMP_W, DISP_TEMP_H, TFT_BLACK);
    tft.setTextColor(tempColor, TFT_BLACK);
    tft.setTextSize(4);
    char buf[16];
    snprintf(buf, sizeof(buf), "%5.1f", currentTemp);
    tft.drawString(buf, DISP_TEMP_X, DISP_TEMP_Y, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("C", DISP_TEMP_UNIT_X, DISP_TEMP_UNIT_Y, 2);

    // Température cible
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Cible: %5.1f", targetTemp);
    tft.drawString(buf, DISP_TARGET_X, DISP_TARGET_Y, 2);

    // SSR1 : label + carré indicateur
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SSR1", DISP_SSR1_LABEL_X, DISP_SSR1_LABEL_Y, 2);
    tft.fillRect(DISP_SSR1_BOX_X, DISP_SSR1_BOX_Y, DISP_SSR1_BOX_W, DISP_SSR1_BOX_H,
                 lastSsr1State ? TFT_RED : TFT_DARKGREY);
    tft.drawRect(DISP_SSR1_BOX_X, DISP_SSR1_BOX_Y, DISP_SSR1_BOX_W, DISP_SSR1_BOX_H, TFT_WHITE);

    // SSR2 : label + barre PWM (y=40 → légèrement sous le label SSR2 à y=40)
    tft.drawString("SSR2", DISP_SSR2_LABEL_X, DISP_SSR2_LABEL_Y, 2);
    tft.drawRect(210, 40, 80, 14, TFT_WHITE);
    int pwmW = (int)(ssr2Pwm / 1023.0 * 78.0);
    tft.fillRect(211, 41, pwmW, 12, TFT_ORANGE);

    // Nom du programme
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(DISP_PROG_NAME_X, DISP_PROG_ROW_Y, DISP_PROG_NAME_W, 20, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%s", programLoaded ? currentProgram.name : "-");
    tft.drawString(buf, DISP_PROG_NAME_X, DISP_PROG_ROW_Y, 2);

    // Étape courante / total
    tft.fillRect(DISP_STEP_X, DISP_PROG_ROW_Y, DISP_STEP_W, 20, TFT_BLACK);
    if (programLoaded)
        snprintf(buf, sizeof(buf), "Etape: %d/%d", currentProgram.currentStep + 1, currentProgram.numSteps);
    else
        snprintf(buf, sizeof(buf), "Etape: -");
    tft.drawString(buf, DISP_STEP_X, DISP_PROG_ROW_Y, 2);

    // Temps écoulé
    tft.fillRect(DISP_TIME_X, DISP_PROG_ROW_Y, DISP_TIME_W, 20, TFT_BLACK);
    if (programRunning)
    {
        unsigned long elapsed = (millis() - programStartMs) / 1000;
        int min = elapsed / 60;
        int sec = elapsed % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d", min, sec);
        tft.drawString(buf, DISP_TIME_X, DISP_PROG_ROW_Y, 2);
    }
    else
    {
        tft.drawString("--:--", DISP_TIME_X, DISP_PROG_ROW_Y, 2);
    }

    // Phase (IDLE / RAMP / HOLD / STAB)
    tft.fillRect(DISP_STATE_X, DISP_PROG_ROW_Y, DISP_STATE_W, 20, TFT_BLACK);
    const char *stateStr = "IDLE";
    if (programRunning && programLoaded)
    {
        if (currentPhase == PHASE_RAMP)      stateStr = "RAMP";
        else if (currentPhase == PHASE_HOLD) stateStr = "HOLD";
        else if (isStabilizing)              stateStr = "STAB";
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(stateStr, DISP_STATE_X, DISP_PROG_ROW_Y, 2);

    // -------------------------------------------------------------------------
    // PARTIE 2 — Zone graphe (idle) ou timeline (programme en cours)
    // -------------------------------------------------------------------------

    int screenW = tft.width();

    if (!programRunning || !programLoaded)
    {
        // --- Mode IDLE : graphe de température circulaire ---
        const int gx = DISP_GRAPH_LEFT_MARGIN;
        const int gy = DISP_GRAPH_TOP_Y;
        const int gw = screenW - DISP_GRAPH_LEFT_MARGIN - DISP_GRAPH_RIGHT_MARGIN;
        const int gh = DISP_GRAPH_HEIGHT;

        tft.fillRect(0, gy, DISP_GRAPH_LEFT_MARGIN, gh, TFT_BLACK);
        tft.fillRect(gx, gy, gw, gh, TFT_DARKGREY);

        float minT = 9999.0, maxT = -9999.0;
        int count = tempSamplesFilled ? GRAPH_W : tempSampleIdx;
        if (count <= 1)
        {
            minT = (float)currentTemp - 5;
            maxT = (float)currentTemp + 5;
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                int idx = tempSamplesFilled ? ((tempSampleIdx + i) % GRAPH_W) : i;
                float v = tempSamples[idx];
                if (v < minT) minT = v;
                if (v > maxT) maxT = v;
            }
            float pad = (maxT - minT) * 0.1f;
            if (pad < 0.5f) pad = 0.5f;
            minT -= pad;
            maxT += pad;
        }
        if (minT >= maxT) maxT = minT + 1.0f;

        // Axes
        tft.drawFastVLine(gx, gy, gh, TFT_WHITE);
        tft.drawFastHLine(gx, gy + gh, gw, TFT_WHITE);

        // Graduations Y (5 niveaux)
        for (int g = 0; g <= 4; ++g)
        {
            int yy = gy + g * (gh / 4);
            tft.drawFastHLine(gx, yy, gw, TFT_BLACK);
            float val = maxT - (maxT - minT) * (float)g / 4.0f;
            String label = String(val, (maxT - minT) < 20 ? 1 : 0);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(label, 2, yy - 6, 1);
        }

        // Tracé de la courbe
        int plotCount = (count > gw) ? gw : count;
        if (plotCount > 1)
        {
            int startIdx = tempSamplesFilled
                ? ((count > gw) ? ((tempSampleIdx - gw + GRAPH_W) % GRAPH_W) : 0)
                : 0;
            int prevX = gx;
            float firstSample = tempSamples[startIdx];
            int prevY = gy + gh - (int)((firstSample - minT) / (maxT - minT) * (gh - 1));
            for (int i = 1; i < plotCount; ++i)
            {
                int idx = tempSamplesFilled ? ((startIdx + i) % GRAPH_W) : i;
                int px = gx + i;
                float s = tempSamples[idx];
                int y = gy + gh - (int)((s - minT) / (maxT - minT) * (gh - 1));
                tft.drawLine(prevX, prevY, px, y, TFT_GREEN);
                prevX = px;
                prevY = y;
            }
        }

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    else
    {
        // --- Mode programme : timeline des étapes ---
        const int gx = DISP_TIMELINE_X;
        const int gy = DISP_TIMELINE_Y;
        const int gw = screenW;
        const int gh = DISP_TIMELINE_H;
        tft.fillRect(gx, gy, gw, gh, TFT_BLACK);

        if (programLoaded && currentProgram.numSteps > 0)
        {
            const int x0 = gx + DISP_TIMELINE_PAD_X;
            const int y0 = gy + DISP_TIMELINE_PAD_Y;
            const int w  = gw - 2 * DISP_TIMELINE_PAD_X;
            const int h  = gh - 2 * DISP_TIMELINE_PAD_Y;

            int totalDuration = 0;
            for (int i = 0; i < currentProgram.numSteps; ++i)
                totalDuration += currentProgram.steps[i].holdTime;

            if (totalDuration > 0)
            {
                unsigned long now = millis();

                // Plage de températures pour l'axe Y
                float minT = 9999.0, maxT = -9999.0;
                for (int i = 0; i < currentProgram.numSteps; ++i)
                {
                    float t0 = (i == 0) ? (float)currentTemp : currentProgram.steps[i - 1].targetTemp;
                    float t1 = currentProgram.steps[i].targetTemp;
                    if (t0 < minT) minT = t0;
                    if (t1 < minT) minT = t1;
                    if (t0 > maxT) maxT = t0;
                    if (t1 > maxT) maxT = t1;
                }
                float pad = (maxT - minT) * 0.1f;
                if (pad < 2) pad = 2;
                minT -= pad;
                maxT += pad;
                if (minT >= maxT) maxT = minT + 1.0f;

                int blockW = (w - (currentProgram.numSteps - 1) * DISP_TIMELINE_STEP_GAP)
                             / currentProgram.numSteps;
                int xBloc = x0;

                for (int i = 0; i < currentProgram.numSteps; ++i)
                {
                    float t0 = (i == 0) ? (float)currentTemp : currentProgram.steps[i - 1].targetTemp;
                    float t1 = currentProgram.steps[i].targetTemp;
                    int y0Bloc = y0 + h - (int)((t0 - minT) / (maxT - minT) * (h - 1));
                    int y1Bloc = y0 + h - (int)((t1 - minT) / (maxT - minT) * (h - 1));

                    if (currentProgram.steps[i].withRamp && currentProgram.steps[i].rampRate > 0)
                    {
                        // Rampe : bloc orange incliné
                        int yMin   = (y1Bloc < y0Bloc) ? y1Bloc : y0Bloc;
                        int height = abs(y1Bloc - y0Bloc);
                        tft.fillRect(xBloc, yMin, blockW, height, TFT_ORANGE);
                        tft.drawRect(xBloc, yMin, blockW, height, TFT_WHITE);

                        if (i == currentProgram.currentStep && programRunning && currentPhase == PHASE_RAMP)
                        {
                            float progress = (t1 != t0) ? (currentTemp - t0) / (t1 - t0) : 0.0f;
                            progress = constrain(progress, 0.0f, 1.0f);
                            int fillH = (int)(height * progress);
                            if (fillH > 0)
                            {
                                if (y1Bloc < y0Bloc) // montée
                                    tft.fillRect(xBloc, y0Bloc - fillH, blockW, fillH, TFT_YELLOW);
                                else                 // descente
                                    tft.fillRect(xBloc, y0Bloc, blockW, fillH, TFT_YELLOW);
                            }
                        }
                    }
                    else
                    {
                        // Palier : barre verte
                        int yPalier = y1Bloc;
                        tft.fillRect(xBloc, yPalier, blockW, 8, TFT_GREEN);
                        tft.drawRect(xBloc, yPalier, blockW, 8, TFT_WHITE);

                        if (i == currentProgram.currentStep && programRunning && currentPhase == PHASE_HOLD)
                        {
                            unsigned long elapsed  = now - phaseStartMs;
                            unsigned long duration = currentProgram.steps[i].holdTime * 60000UL;
                            float progress = (duration > 0) ? (float)elapsed / (float)duration : 0.0f;
                            progress = constrain(progress, 0.0f, 1.0f);
                            int fillW = (int)(blockW * progress);
                            if (fillW > 0)
                                tft.fillRect(xBloc, yPalier, fillW, 8, TFT_BLUE);
                        }
                    }

                    // Étiquettes : température + durée
                    char lbuf[16];
                    tft.setTextColor(TFT_BLACK, TFT_GREEN);
                    tft.setTextSize(1);
                    snprintf(lbuf, sizeof(lbuf), "%dC", (int)t1);
                    tft.drawCentreString(lbuf, xBloc + blockW / 2, y1Bloc - 18, 2);
                    snprintf(lbuf, sizeof(lbuf), "%dmin", (int)currentProgram.steps[i].holdTime);
                    tft.drawCentreString(lbuf, xBloc + blockW / 2, y1Bloc + 10, 1);

                    xBloc += blockW + DISP_TIMELINE_STEP_GAP;
                }
            }
        }

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    lastUpdate = millis();
}

// ---------------------------------------------------------------------------
// displayStartupScreen — Informations au démarrage (IP, WiFi, NTP)
// ---------------------------------------------------------------------------
void displayStartupScreen()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Four Verrier", 10, 10, 4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    String versionText = "Firmware: " + String(FIRMWARE_VERSION);
    tft.drawString(versionText, 10, 45, 2);
    tft.drawFastHLine(10, 70, 300, TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int yPos = 85;

    // Adresse IP
    tft.drawString("Adresse IP:", 10, yPos, 2);
    String ipAddress;
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        ipAddress = WiFi.localIP().toString();
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        ipAddress = WiFi.softAPIP().toString();
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    }
    else
    {
        ipAddress = "Non connecte";
        tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    tft.drawString(ipAddress, 130, yPos, 2);
    yPos += 25;

    // Signal WiFi
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Signal WiFi:", 10, yPos, 2);
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        int32_t rssi = WiFi.RSSI();
        if (rssi > -50)      tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else if (rssi > -70) tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        else                 tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(String(rssi) + " dBm", 130, yPos, 2);

        // Barres de signal (5 barres, seuils -90/-80/-70/-60/-50 dBm)
        const int barX = 240, barY = yPos, barMaxH = 20, barSpacing = 8;
        for (int i = 0; i < 5; ++i)
        {
            int barH = (i + 1) * (barMaxH / 5);
            int threshold = -90 + i * 10;
            uint16_t color = (rssi >= threshold) ? TFT_GREEN : TFT_DARKGREY;
            tft.fillRect(barX + i * barSpacing, barY + (barMaxH - barH), 6, barH, color);
        }
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Mode AP", 130, yPos, 2);
    }
    else
    {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Non dispo", 130, yPos, 2);
    }
    yPos += 25;

    // Heure NTP
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Heure NTP:", 10, yPos, 2);
    time_t now = time(nullptr);
    if (now >= (time_t)1600000000)
    {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(timeStr, 10, yPos + 20, 2);
    }
    else
    {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("Non synchronise", 10, yPos + 20, 2);
    }

    yPos += 60;
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Demarrage en cours...", 10, yPos, 2);

    delay(5000);
}
