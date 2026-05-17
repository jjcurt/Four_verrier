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
extern uint16_t      ssr2Pwm;
extern bool          programLoaded;
extern bool          programRunning;
extern FiringProgram currentProgram;
extern unsigned long programStartMs;
extern double        programStartTemp;
extern ProgramPhase      currentPhase;
extern bool          isStabilizing;
extern unsigned long phaseStartMs;
extern unsigned long stepStartMs;
extern float         rampStartTemp;
extern double        stabilizingToleranceStrict;
extern double        filteredTemp;
extern float         tempSamples[];
extern int           tempSampleIdx;
extern bool          tempSamplesFilled;
extern unsigned long idleGraphUpdateInterval;

// ---------------------------------------------------------------------------
// drawStaticUI — Cadre invariant (une seule fois au démarrage ou après reset)
// ---------------------------------------------------------------------------
void drawStaticUI()
{
    tft.fillScreen(DISP_BLACK);
    tft.setTextColor(DISP_WHITE, DISP_BLACK);
    tft.setTextSize(1);

    // Séparateurs horizontaux
    tft.drawFastHLine(0, 57,             SCREEN_WIDTH, DISP_DARKGREY);
    tft.drawFastHLine(0, DISP_GRAPH_TOP_Y - 1, SCREEN_WIDTH, DISP_DARKGREY);

    // Étiquettes zone température
    tft.drawString("Four:",  DISP_TEMP_LABEL_X,   DISP_TEMP_LABEL_Y,   2);
    tft.drawString("C",      DISP_TEMP_UNIT_X,     DISP_TEMP_UNIT_Y,    2);
    tft.drawString("Cible:", DISP_TARGET_LABEL_X,  DISP_TARGET_LABEL_Y, 2);
    tft.drawString("C",      DISP_TARGET_UNIT_X,   DISP_TARGET_UNIT_Y,  2);

    // Étiquette SSR2 + cadre barre PWM (SSR1 supprimé: redondant avec état programme)
    tft.drawString("SSR2", DISP_SSR2_LABEL_X, DISP_SSR2_LABEL_Y, 2);
    tft.drawRect(DISP_SSR2_BAR_X, DISP_SSR2_BAR_Y, DISP_SSR2_BAR_W, DISP_SSR2_BAR_H, DISP_WHITE);

    // Étiquettes zone programme
    tft.drawString("Prog:",  DISP_PROG_LABEL_X,  DISP_PROG_ROW_Y,    2);
    tft.drawString("Etape:", DISP_STEP_LABEL_X,  DISP_STATUS_ROW_Y,  2);
    // "Temps:" supprimé : remplacé par couleurs (orange=rampe, vert=hold)
}

// ---------------------------------------------------------------------------
// updateDisplay — Éléments dynamiques (appelé à chaque loop())
// ---------------------------------------------------------------------------
void updateDisplay(bool force)
{
    static unsigned long lastTopUpdate   = 0;
    static unsigned long lastGraphUpdate = 0;

    const unsigned long now = millis();
    bool doTop   = force || (now - lastTopUpdate   >= 250);
    bool doGraph = force || (now - lastGraphUpdate >= idleGraphUpdateInterval);

    if (!doTop && !doGraph)
        return;

    // -------------------------------------------------------------------------
    // PARTIE 1 — Bandeau supérieur (température, SSR, état programme)
    // -------------------------------------------------------------------------
    if (doTop)
    {
        char buf[16];

        // --- Température four (valeur colorée selon état) ---
        // Refroidissement naturel : gradient chaud→froid selon température réelle.
        // Descente contrôlée (rampe descendante en programme) : magenta chaud.
        auto coolingColor = [](float t) -> uint16_t {
            if (t > 150.0f) return DISP_RED;
            if (t > 100.0f) return DISP_ORANGE;
            if (t >  60.0f) return DISP_YELLOW;
            if (t >  40.0f) return DISP_GREEN;
            return DISP_CYAN;
        };

        uint16_t tempColor;
        if (!programRunning)
        {
            // Refroidissement naturel post-programme
            tempColor = (currentTemp > 40.0f) ? coolingColor(currentTemp) : DISP_WHITE;
        }
        else
        {
            switch (currentPhase)
            {
            case PHASE_RAMP:
                tempColor = (currentProgram.steps[currentProgram.currentStep].targetTemp
                             < rampStartTemp) ? DISP_MAGENTA : DISP_ORANGE;
                break;
            case PHASE_HOLD:
                if      (currentTemp < targetTemp - stabilizingToleranceStrict) tempColor = DISP_CYAN;
                else if (currentTemp > targetTemp + stabilizingToleranceStrict) tempColor = DISP_RED;
                else                                                             tempColor = DISP_GREEN;
                break;
            default:
                tempColor = DISP_WHITE;
            }
        }
        tft.fillRect(DISP_TEMP_X, DISP_TEMP_Y, DISP_TEMP_W, DISP_TEMP_H, DISP_BLACK);
        tft.setTextColor(tempColor, DISP_BLACK);
        snprintf(buf, sizeof(buf), "%5.1f", filteredTemp);
        tft.drawString(buf, DISP_TEMP_X, DISP_TEMP_Y, 4);

        // --- Température cible (jaune, label statique) ---
        tft.fillRect(DISP_TARGET_X, DISP_TARGET_Y, DISP_TARGET_W, DISP_TARGET_H, DISP_BLACK);
        tft.setTextColor(DISP_YELLOW, DISP_BLACK);
        snprintf(buf, sizeof(buf), "%5.1f", targetTemp);
        tft.drawString(buf, DISP_TARGET_X, DISP_TARGET_Y, 4);

        // --- Phase (haut droite, font 4, colorée — remplace SSR1) ---
        {
            const char *phaseStr = "IDLE";
            uint16_t phaseColor  = DISP_DARKGREY;
            if (programRunning && programLoaded)
            {
                if      (currentPhase == PHASE_RAMP && isStabilizing) { phaseStr = "STAB"; phaseColor = DISP_YELLOW; }
                else if (currentPhase == PHASE_RAMP)                   { phaseStr = "RAMP"; phaseColor = DISP_ORANGE; }
                else if (currentPhase == PHASE_HOLD)                   { phaseStr = "HOLD"; phaseColor = DISP_GREEN;  }
            }
            tft.fillRect(DISP_PHASE_TOP_X, DISP_PHASE_TOP_Y, DISP_PHASE_TOP_W, DISP_PHASE_TOP_H, DISP_BLACK);
            tft.setTextColor(phaseColor, DISP_BLACK);
            tft.drawString(phaseStr, DISP_PHASE_TOP_X, DISP_PHASE_TOP_Y, 4);
        }

        // --- SSR2 barre PWM (cadre statique, remplissage dynamique) ---
        int pwmW = (int)(ssr2Pwm / 1023.0 * DISP_SSR2_FILL_MAX);
        tft.fillRect(DISP_SSR2_FILL_X, DISP_SSR2_FILL_Y,
                     DISP_SSR2_FILL_MAX, DISP_SSR2_BAR_H - 2, DISP_BLACK);
        if (pwmW > 0)
            tft.fillRect(DISP_SSR2_FILL_X, DISP_SSR2_FILL_Y,
                         pwmW, DISP_SSR2_BAR_H - 2, DISP_ORANGE);

        // --- Nom du programme (largeur réduite pour laisser place au temps total) ---
        tft.fillRect(DISP_PROG_NAME_X, DISP_PROG_ROW_Y, DISP_PROG_NAME_W, 16, DISP_BLACK);
        tft.setTextColor(programRunning ? DISP_GREEN : DISP_WHITE, DISP_BLACK);
        tft.drawString(programLoaded ? currentProgram.name : "Idle",
                       DISP_PROG_NAME_X, DISP_PROG_ROW_Y, 2);

        // --- Temps total écoulé programme (ligne Prog, droite, blanc) ---
        tft.fillRect(DISP_PROG_TOTAL_X, DISP_PROG_ROW_Y, DISP_PROG_TOTAL_W, 16, DISP_BLACK);
        if (programRunning)
        {
            tft.setTextColor(DISP_WHITE, DISP_BLACK);
            unsigned long tot = (millis() - programStartMs) / 1000;
            if (tot < 3600)
                snprintf(buf, sizeof(buf), "%lu:%02lu", tot / 60, tot % 60);
            else
                snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
                         tot / 3600, (tot % 3600) / 60, tot % 60);
            tft.drawString(buf, DISP_PROG_TOTAL_X, DISP_PROG_ROW_Y, 2);
        }

        // --- Étape ---
        tft.fillRect(DISP_STEP_X, DISP_STATUS_ROW_Y, DISP_STEP_W, 16, DISP_BLACK);
        tft.setTextColor(DISP_WHITE, DISP_BLACK);
        if (programLoaded)
            snprintf(buf, sizeof(buf), "%d/%d",
                     currentProgram.currentStep + 1, currentProgram.numSteps);
        else
            snprintf(buf, sizeof(buf), "0");
        tft.drawString(buf, DISP_STEP_X, DISP_STATUS_ROW_Y, 2);

        // --- Temps rampe écoulé (orange, MM:SS, compte en avant) ---
        tft.fillRect(DISP_STEP_TIME_X, DISP_STATUS_ROW_Y, DISP_STEP_TIME_W, 16, DISP_BLACK);
        if (programRunning && programLoaded)
        {
            tft.setTextColor(DISP_ORANGE, DISP_BLACK);
            unsigned long rampSec = 0;
            if (currentPhase == PHASE_RAMP)
                rampSec = (millis() - phaseStartMs) / 1000;
            else if (currentPhase == PHASE_HOLD && phaseStartMs > stepStartMs)
                rampSec = (phaseStartMs - stepStartMs) / 1000;  // durée rampe figée
            snprintf(buf, sizeof(buf), "%lu:%02lu", rampSec / 60, rampSec % 60);
            tft.drawString(buf, DISP_STEP_TIME_X, DISP_STATUS_ROW_Y, 2);
        }

        // --- Décompteur HOLD restant (vert, MM:SS, compte à rebours) ---
        tft.fillRect(DISP_HOLD_TIME_X, DISP_STATUS_ROW_Y, DISP_HOLD_TIME_W, 16, DISP_BLACK);
        if (programRunning && programLoaded)
        {
            tft.setTextColor(DISP_GREEN, DISP_BLACK);
            uint8_t si = (currentProgram.currentStep < currentProgram.numSteps)
                         ? currentProgram.currentStep : currentProgram.numSteps - 1;
            unsigned long holdTotal = (unsigned long)currentProgram.steps[si].holdTime * 60UL;
            unsigned long holdRemaining = holdTotal;
            if (currentPhase == PHASE_HOLD)
            {
                unsigned long holdElapsed = (millis() - phaseStartMs) / 1000;
                holdRemaining = (holdElapsed < holdTotal) ? holdTotal - holdElapsed : 0;
            }
            snprintf(buf, sizeof(buf), "%lu:%02lu", holdRemaining / 60, holdRemaining % 60);
            tft.drawString(buf, DISP_HOLD_TIME_X, DISP_STATUS_ROW_Y, 2);
        }

        lastTopUpdate = millis();
    }

    // -------------------------------------------------------------------------
    // PARTIE 2 — Zone graphe (idle) ou timeline (programme en cours)
    // -------------------------------------------------------------------------
    if (doGraph)
    {
    int screenW = tft.width();

    if (!programRunning || !programLoaded)
    {
        // --- Mode IDLE : graphe de température ---
        const int gx     = DISP_GRAPH_LEFT_MARGIN;
        const int gy     = DISP_GRAPH_TOP_Y;
        const int gw     = screenW - DISP_GRAPH_LEFT_MARGIN - DISP_GRAPH_RIGHT_MARGIN;
        const int gh     = DISP_GRAPH_HEIGHT;
        const int plotH  = gh - DISP_GRAPH_XLABEL_H;   // zone tracé (hors labels X)
        const int xAxisY = gy + plotH;                  // y de la ligne X

        // Fonds
        tft.fillRect(0,  gy, gx, gh,    DISP_BLACK);     // marge gauche labels Y
        tft.fillRect(gx, gy, gw, plotH, DISP_BLACK);      // zone tracé
        tft.fillRect(gx, xAxisY, gw, DISP_GRAPH_XLABEL_H, DISP_BLACK); // zone labels X

        // Lissage affichage uniquement — moyenne glissante 3 pts sur le buffer circulaire.
        // N'affecte ni filteredTemp ni le PID.
        auto smoothSample = [&](int baseStart, int pos, int total) -> float {
            float sum = 0.0f; int cnt = 0;
            for (int k = -2; k <= 2; ++k)
            {
                int p = pos + k;
                if (p < 0 || p >= total) continue;
                sum += tempSamples[tempSamplesFilled ? ((baseStart + p) % GRAPH_W) : p];
                ++cnt;
            }
            return cnt > 0 ? sum / cnt : 0.0f;
        };

        float minT = 9999.0, maxT = -9999.0;
        int count = tempSamplesFilled ? GRAPH_W : tempSampleIdx;
        if (count <= 1)
        {
            minT = (float)currentTemp - 5;
            maxT = (float)currentTemp + 5;
        }
        else
        {
            int mmStart = tempSamplesFilled ? tempSampleIdx : 0;
            for (int i = 0; i < count; ++i)
            {
                float v = smoothSample(mmStart, i, count);
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
        tft.drawFastVLine(gx, gy,     plotH, DISP_WHITE);  // axe Y
        tft.drawFastHLine(gx, xAxisY, gw,    DISP_WHITE);  // axe X (visible !)

        // Graduations Y (5 niveaux)
        for (int g = 0; g <= 4; ++g)
        {
            int yy = gy + g * (plotH / 4);
            tft.drawFastHLine(gx, yy, gw, DISP_DARKGREY);
            float val = maxT - (maxT - minT) * (float)g / 4.0f;
            String label = String(val, (maxT - minT) < 20 ? 1 : 0);
            tft.setTextColor(DISP_WHITE, DISP_BLACK);
            int labelY = max(yy - 6, gy + 1);
            tft.drawString(label, 2, labelY, 1);
        }

        // Graduations X (toutes les minutes — buffer 300s à 1s/sample)
        {
            float pxPerMin = (float)gw * 60.0f / GRAPH_W;  // ~54.6 px/min
            for (int t = 0; t <= 5; ++t)
            {
                int xTick = gx + gw - 1 - (int)(t * pxPerMin);
                if (xTick < gx) continue;
                tft.drawFastVLine(xTick, xAxisY, 4, DISP_WHITE);
                char tLabel[6];
                if (t == 0) snprintf(tLabel, sizeof(tLabel), "0");
                else        snprintf(tLabel, sizeof(tLabel), "-%dm", t);
                tft.setTextColor(DISP_WHITE, DISP_BLACK);
                tft.drawCentreString(tLabel, xTick, xAxisY + 4, 1);
            }
        }

        // Tracé de la courbe (alignée à droite : bord droit = maintenant)
        int plotCount = (count > gw) ? gw : count;
        if (plotCount > 1)
        {
            int startIdx = tempSamplesFilled
                ? ((count > gw) ? ((tempSampleIdx - gw + GRAPH_W) % GRAPH_W) : 0)
                : 0;
            int xOffset = gw - plotCount;   // décalage pour ancrer la courbe à droite
            int prevX = gx + xOffset;
            float firstSample = smoothSample(startIdx, 0, plotCount);
            int prevY = xAxisY - 1 - (int)((firstSample - minT) / (maxT - minT) * (plotH - 1));
            prevY = constrain(prevY, gy, xAxisY - 1);
            for (int i = 1; i < plotCount; ++i)
            {
                int px = gx + xOffset + i;
                float s = smoothSample(startIdx, i, plotCount);
                int y = xAxisY - 1 - (int)((s - minT) / (maxT - minT) * (plotH - 1));
                y = constrain(y, gy, xAxisY - 1);
                tft.drawLine(prevX, prevY, px, y, DISP_GREEN);
                prevX = px;
                prevY = y;
            }
        }

        tft.setTextColor(DISP_WHITE, DISP_BLACK);
    }
    else
    {
        // --- Mode programme : timeline des étapes ---
        const int gx = DISP_TIMELINE_X;
        const int gy = DISP_TIMELINE_Y;
        const int gw = screenW;
        const int gh = DISP_TIMELINE_H;
        tft.fillRect(gx, gy, gw, gh, DISP_BLACK);

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
                    float t0 = (i == 0) ? (float)(programStartTemp > 0 ? programStartTemp : 25.0) : currentProgram.steps[i - 1].targetTemp;
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
                    float t0 = (i == 0) ? (float)(programStartTemp > 0 ? programStartTemp : 25.0) : currentProgram.steps[i - 1].targetTemp;
                    float t1 = currentProgram.steps[i].targetTemp;
                    int y0Bloc = y0 + h - (int)((t0 - minT) / (maxT - minT) * (h - 1));
                    int y1Bloc = y0 + h - (int)((t1 - minT) / (maxT - minT) * (h - 1));

                    if (currentProgram.steps[i].withRamp && currentProgram.steps[i].rampRate > 0)
                    {
                        // Rampe : bloc orange incliné
                        int yMin   = (y1Bloc < y0Bloc) ? y1Bloc : y0Bloc;
                        int height = abs(y1Bloc - y0Bloc);
                        tft.fillRect(xBloc, yMin, blockW, height, DISP_ORANGE);
                        tft.drawRect(xBloc, yMin, blockW, height, DISP_WHITE);

                        if (i == currentProgram.currentStep && programRunning && currentPhase == PHASE_RAMP)
                        {
                            float progress = (t1 != t0) ? (currentTemp - t0) / (t1 - t0) : 0.0f;
                            progress = constrain(progress, 0.0f, 1.0f);
                            int fillH = (int)(height * progress);
                            if (fillH > 0)
                            {
                                if (y1Bloc < y0Bloc) // montée
                                    tft.fillRect(xBloc, y0Bloc - fillH, blockW, fillH, DISP_YELLOW);
                                else                 // descente
                                    tft.fillRect(xBloc, y0Bloc, blockW, fillH, DISP_YELLOW);
                            }
                        }
                    }
                    else
                    {
                        // Palier : barre verte
                        int yPalier = y1Bloc;
                        tft.fillRect(xBloc, yPalier, blockW, 8, DISP_GREEN);
                        tft.drawRect(xBloc, yPalier, blockW, 8, DISP_WHITE);

                        if (i == currentProgram.currentStep && programRunning && currentPhase == PHASE_HOLD)
                        {
                            unsigned long elapsed  = now - phaseStartMs;
                            unsigned long duration = currentProgram.steps[i].holdTime * 60000UL;
                            float progress = (duration > 0) ? (float)elapsed / (float)duration : 0.0f;
                            progress = constrain(progress, 0.0f, 1.0f);
                            int fillW = (int)(blockW * progress);
                            if (fillW > 0)
                                tft.fillRect(xBloc, yPalier, fillW, 8, DISP_BLUE);
                        }
                    }

                    // Étiquettes : température + durée
                    char lbuf[16];
                    tft.setTextColor(DISP_BLACK, DISP_GREEN);
                    tft.setTextSize(1);
                    snprintf(lbuf, sizeof(lbuf), "%dC", (int)t1);
                    tft.drawCentreString(lbuf, xBloc + blockW / 2, y1Bloc - 18, 2);
                    snprintf(lbuf, sizeof(lbuf), "%dmin", (int)currentProgram.steps[i].holdTime);
                    tft.drawCentreString(lbuf, xBloc + blockW / 2, y1Bloc + 10, 1);

                    xBloc += blockW + DISP_TIMELINE_STEP_GAP;
                }
            }
        }

        tft.setTextColor(DISP_WHITE, DISP_BLACK);
    }

        lastGraphUpdate = millis();
    } // if (doGraph)
}

// ---------------------------------------------------------------------------
// displayStartupScreen — Informations au démarrage (IP, WiFi, NTP)
// ---------------------------------------------------------------------------
void displayStartupScreen()
{
    tft.fillScreen(DISP_BLACK);
    tft.setTextColor(DISP_WHITE, DISP_BLACK);
    tft.setTextSize(1);
    tft.drawString("Four Verrier", 10, 10, 4);
    tft.setTextColor(DISP_YELLOW, DISP_BLACK);
    String versionText = "Firmware: " + String(FIRMWARE_VERSION);
    tft.drawString(versionText, 10, 45, 2);
    tft.drawFastHLine(10, 70, 300, DISP_DARKGREY);

    tft.setTextColor(DISP_WHITE, DISP_BLACK);
    int yPos = 85;

    // Adresse IP
    tft.drawString("Adresse IP:", 10, yPos, 2);
    String ipAddress;
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        ipAddress = WiFi.localIP().toString();
        tft.setTextColor(DISP_GREEN, DISP_BLACK);
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        ipAddress = WiFi.softAPIP().toString();
        tft.setTextColor(DISP_YELLOW, DISP_BLACK);
    }
    else
    {
        ipAddress = "Non connecte";
        tft.setTextColor(DISP_RED, DISP_BLACK);
    }
    tft.drawString(ipAddress, 130, yPos, 2);
    yPos += 25;

    // Signal WiFi
    tft.setTextColor(DISP_WHITE, DISP_BLACK);
    tft.drawString("Signal WiFi:", 10, yPos, 2);
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
    {
        int32_t rssi = WiFi.RSSI();
        if (rssi > -50)      tft.setTextColor(DISP_GREEN, DISP_BLACK);
        else if (rssi > -70) tft.setTextColor(DISP_YELLOW, DISP_BLACK);
        else                 tft.setTextColor(DISP_ORANGE, DISP_BLACK);
        tft.drawString(String(rssi) + " dBm", 130, yPos, 2);

        // Barres de signal (5 barres, seuils -90/-80/-70/-60/-50 dBm)
        const int barX = 240, barY = yPos, barMaxH = 20, barSpacing = 8;
        for (int i = 0; i < 5; ++i)
        {
            int barH = (i + 1) * (barMaxH / 5);
            int threshold = -90 + i * 10;
            uint16_t color = (rssi >= threshold) ? DISP_GREEN : DISP_DARKGREY;
            tft.fillRect(barX + i * barSpacing, barY + (barMaxH - barH), 6, barH, color);
        }
    }
    else if (WiFi.getMode() == WIFI_AP)
    {
        tft.setTextColor(DISP_YELLOW, DISP_BLACK);
        tft.drawString("Mode AP", 130, yPos, 2);
    }
    else
    {
        tft.setTextColor(DISP_RED, DISP_BLACK);
        tft.drawString("Non dispo", 130, yPos, 2);
    }
    yPos += 25;

    // Heure NTP
    tft.setTextColor(DISP_WHITE, DISP_BLACK);
    tft.drawString("Heure NTP:", 10, yPos, 2);
    time_t now = time(nullptr);
    if (now >= (time_t)1600000000)
    {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
        tft.setTextColor(DISP_GREEN, DISP_BLACK);
        tft.drawString(timeStr, 10, yPos + 20, 2);
    }
    else
    {
        tft.setTextColor(DISP_ORANGE, DISP_BLACK);
        tft.drawString("Non synchronise", 10, yPos + 20, 2);
    }

    yPos += 60;
    tft.setTextColor(DISP_DARKGREY, DISP_BLACK);
    tft.drawString("Demarrage en cours...", 10, yPos, 2);

    delay(5000);
}
