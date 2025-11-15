#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();
int x, y, count = 1;
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE2 2
#define FONT_SIZE4 4

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define a1 -24.690260649809815
#define a2 0.09051783662445466
#define a3 -0.0003958564871839598
#define b1 -30.69652183524532
#define b2 0.0008835593624712326
#define b3 0.07414778741434636

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
int centreX;

void setup()
{
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(3);

    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(3);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.drawRect(15, 15, 285, 205, TFT_RED);
    tft.fillRect(120, 100, 50, 50, TFT_YELLOW);
    tft.fillRect(130, 50, 30, 30, TFT_BLUE);
    tft.fillRect(130, 165, 30, 30, TFT_BLUE);
    tft.setTextColor(TFT_YELLOW, TFT_BLUE, true);
    tft.drawChar('+', 138, 52, 4);
    tft.drawChar('-', 138, 167, 4);
    tft.setTextColor(TFT_RED, TFT_YELLOW, true);
    tft.drawNumber(count, 135, 114, 4);

    tft.fillRect(230, 180, 60, 30, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN, true);
    tft.drawString("START", 238, 185, 2);

    centreX = SCREEN_WIDTH / 2;
    tft.setTextColor(TFT_YELLOW, TFT_BLACK, true);
    tft.drawCentreString(" TABLE de muliplication de ", centreX, 25, 2);

    tft.drawSpot(50., 50., 1, TFT_CYAN);
    tft.drawFastHLine(5, 10, 10, TFT_WHITE);
    tft.drawFastVLine(10, 5, 10, TFT_WHITE);
    tft.drawFastHLine(304, 234, 10, TFT_WHITE);
    tft.drawFastVLine(309, 229, 10, TFT_WHITE);

    tft.drawFastHLine(5, 234, 10, TFT_WHITE);
    tft.drawFastVLine(10, 229, 10, TFT_WHITE);
    tft.drawFastHLine(304, 5, 10, TFT_WHITE);
    tft.drawFastVLine(309, 0, 10, TFT_WHITE);
}

void loop()
{
    // tft.drawCentreString("         ",centreX,25,4);
    if (touchscreen.tirqTouched() && touchscreen.touched())
    {
        TS_Point p = touchscreen.getPoint();

        x = round(a1 + a2 * p.x + a3 * p.y);
        y = round(b1 + b2 * p.x + b3 * p.y);
        // x = map(p.x,200,3700,1,SCREEN_WIDTH);
        // y = map(p.y,240,3800,1,SCREEN_HEIGHT);

        Serial.printf("px=%d , py=%d\t", p.x, p.y);
        Serial.printf("x=%d , y=%d\n", x, y);

        if (x > 130 && x < 160 && y > 50 && y < 80)
        {
            count++;
            if (count > 12)
            {
                count = 1;
            }
        }

        if (x > 130 && x < 160 && y > 165 && y < 195)
        {
            count--;
            if (count < 1)
            {
                count = 12;
            }
        }
        tft.fillRect(120, 100, 50, 50, TFT_YELLOW);
        tft.setTextColor(TFT_RED, TFT_YELLOW, true);
        tft.drawNumber(count, 135, 114, 4);

        // start
        if (x > 230 && x < 290 && y > 180 && y < 210)
        {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            int xcor = 50;
            int ycor = 20;

            for (int j = 1; j < 12; j++)
            {
                int m = j * count;
                String val = String(j) + " x " + String(count) + " = " + String(m);
                tft.drawString(val, xcor, ycor, 2);
                ycor += 16;
            }

            delay(10000);
            count = 1;
            setup();
        }
        delay(100);
    }
}
