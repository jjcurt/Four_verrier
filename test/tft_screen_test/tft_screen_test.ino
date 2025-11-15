#include <TFT_eSPI.h>
#include <SPI.h>
#include <TFT_eWidget.h>   

#define TFTWIDTH 320
#define TFTHEIGHT 240

// définition des panneaux sur l'écran TFT
#define TOP_PANEL_X 0
#define TOP_PANEL_Y 0
#define TOP_PANEL_TEXT_SIZE 1
#define TOP_PANEL_WIDTH (TFTWIDTH)
#define TOP_PANEL_HEIGHT (8 * TOP_PANEL_TEXT_SIZE + 4)
#define TOP_PANEL_BG_COLOR TFT_BLUE
#define TOP_PANEL_TEXT_COLOR TFT_WHITE
#define TOP_PANEL_FONT 1
#define TOP_PANEL_BORDER_COLOR TFT_BLUE
#define TOP_PANEL_BORDER_SIZE 1

#define BOTTOM_PANEL_X 0
#define BOTTOM_PANEL_TEXT_SIZE 1
#define BOTTOM_PANEL_NB_LINES 2
#define BOTTOM_PANEL_HEIGHT (BOTTOM_PANEL_NB_LINES * (8 * BOTTOM_PANEL_TEXT_SIZE + 2) + 4)
#define BOTTOM_PANEL_Y (TFTHEIGHT - BOTTOM_PANEL_HEIGHT)
#define BOTTOM_PANEL_WIDTH (TFTWIDTH)
#define BOTTOM_PANEL_BG_COLOR TFT_NAVY
#define BOTTOM_PANEL_TEXT_COLOR TFT_CYAN
#define BOTTOM_PANEL_FONT 1
#define BOTTOM_PANEL_BORDER_COLOR TFT_NAVY
#define BOTTOM_PANEL_BORDER_SIZE 1

#define LEFT_PANEL_X 1
#define LEFT_PANEL_Y (TOP_PANEL_Y + TOP_PANEL_HEIGHT + 2)
#define LEFT_PANEL_TXT_MAX_LEN 8
#define LEFT_PANEL_WIDTH (7 * LEFT_PANEL_TXT_MAX_LEN)
#define LEFT_PANEL_HEIGHT (TFTHEIGHT - TOP_PANEL_HEIGHT - BOTTOM_PANEL_HEIGHT - 2)
#define LEFT_PANEL_BG_COLOR TFT_DARKGREY
#define LEFT_PANEL_TEXT_SIZE 1
#define LEFT_PANEL_TEXT_COLOR TFT_BLACK
#define LEFT_PANEL_FONT 1
#define LEFT_PANEL_BORDER_COLOR TFT_BLACK
#define LEFT_PANEL_BORDER_SIZE -1

#define MAIN_PANEL_X (LEFT_PANEL_X + LEFT_PANEL_WIDTH + 2)
#define MAIN_PANEL_Y (TOP_PANEL_Y + TOP_PANEL_HEIGHT + 1)
#define MAIN_PANEL_WIDTH (TFTWIDTH - MAIN_PANEL_X - 1)
#define MAIN_PANEL_HEIGHT (TFTHEIGHT - MAIN_PANEL_Y - BOTTOM_PANEL_HEIGHT - 2)
#define MAIN_PANEL_BG_COLOR TFT_LIGHTGREY
#define MAIN_PANEL_TEXT_SIZE 1
#define MAIN_PANEL_TEXT_COLOR TFT_NAVY
#define MAIN_PANEL_FONT 2
#define MAIN_PANEL_BORDER_COLOR TFT_DARKGREY
#define MAIN_PANEL_BORDER_SIZE 1

#define LOG_PANEL_X 5
#define LOG_PANEL_Y 1
#define LOG_PANEL_TEXT_SIZE 1
#define LOG_PANEL_WIDTH (TFTWIDTH - 10)
#define LOG_PANEL_HEIGHT (TFTHEIGHT - 2)
#define LOG_PANEL_BG_COLOR TFT_BLACK
#define LOG_PANEL_TEXT_COLOR TFT_WHITE
#define LOG_PANEL_FONT 1
#define LOG_PANEL_BORDER_COLOR TFT_GREEN
#define LOG_PANEL_BORDER_SIZE -1
#define LABEL_SIZE 16
#define VALUE_SIZE 16

typedef struct {
  uint16_t posX, posY, largeur, hauteur;
  uint8_t txtSize, txtFont;
  uint32_t bgColor, borderColor;
  uint16_t txtColor;
  short borderSize;
} tVueConfig;

typedef struct {
  const char *label;
  char value[VALUE_SIZE];
  uint16_t x, y, w, h;
  uint8_t txtSize, txtFont;
  uint16_t labelColor, valueColor;
} tInfoTFT;


TFT_eSPI tft = TFT_eSPI();

// Configuration du buffer
const int MAX_LINES = 24;  // nombre de lignes en mémoire (ajustable)
const int LINE_LEN = 128;  // caractères max par ligne (incluant '\0')

char **linesBuf;   //[MAX_LINES][LINE_LEN];
int bufHead = 0;   // prochaine position d'écriture
int bufCount = 0;  // nombre de lignes stockées

int textSize = 1;
int lineHeight;
int topLineIndex = 0;  // index logique de la ligne affichée en haut

unsigned long lastMillis = 0;
unsigned long lastAdd = 0;



tVueConfig vueLog = { LOG_PANEL_X,
                      LOG_PANEL_Y,
                      LOG_PANEL_WIDTH,
                      LOG_PANEL_HEIGHT,
                      LOG_PANEL_TEXT_SIZE,
                      LOG_PANEL_FONT,
                      LOG_PANEL_BG_COLOR,
                      LOG_PANEL_BORDER_COLOR,
                      LOG_PANEL_TEXT_COLOR,
                      LOG_PANEL_BORDER_SIZE };

tVueConfig vueTop = { TOP_PANEL_X,
                      TOP_PANEL_Y,
                      TOP_PANEL_WIDTH,
                      TOP_PANEL_HEIGHT,
                      TOP_PANEL_TEXT_SIZE,
                      TOP_PANEL_FONT,
                      TOP_PANEL_BG_COLOR,
                      TOP_PANEL_BORDER_COLOR,
                      TOP_PANEL_TEXT_COLOR,
                      TOP_PANEL_BORDER_SIZE };

tVueConfig vueBottom = { BOTTOM_PANEL_X,
                         BOTTOM_PANEL_Y,
                         BOTTOM_PANEL_WIDTH,
                         BOTTOM_PANEL_HEIGHT,
                         BOTTOM_PANEL_TEXT_SIZE,
                         BOTTOM_PANEL_FONT,
                         BOTTOM_PANEL_BG_COLOR,
                         BOTTOM_PANEL_BORDER_COLOR,
                         BOTTOM_PANEL_TEXT_COLOR,
                         BOTTOM_PANEL_BORDER_SIZE };

tVueConfig vueLeft = { LEFT_PANEL_X,
                       LEFT_PANEL_Y,
                       LEFT_PANEL_WIDTH,
                       LEFT_PANEL_HEIGHT,
                       LEFT_PANEL_TEXT_SIZE,
                       LEFT_PANEL_FONT,
                       LEFT_PANEL_BG_COLOR,
                       LEFT_PANEL_BORDER_COLOR,
                       LEFT_PANEL_TEXT_COLOR,
                       LEFT_PANEL_BORDER_SIZE };

tVueConfig vueMain = { MAIN_PANEL_X,              //posX
                       MAIN_PANEL_Y,              //posY
                       MAIN_PANEL_WIDTH,          //largeur
                       MAIN_PANEL_HEIGHT,         //hauteur
                       MAIN_PANEL_TEXT_SIZE,      //txtSize
                       MAIN_PANEL_FONT,           //txtFont
                       MAIN_PANEL_BG_COLOR,       //bgColor
                       MAIN_PANEL_BORDER_COLOR,   //borederColor
                       MAIN_PANEL_TEXT_COLOR,     //txtColor
                       MAIN_PANEL_BORDER_SIZE };  //borderSize


void setVue(tVueConfig *params) {
  tft.resetViewport();
  yield();
  tft.setViewport(params->posX, params->posY, params->largeur, params->hauteur);
  tft.fillScreen(params->bgColor);
  yield();
  if (params->borderSize != 0) tft.frameViewport(params->borderColor, params->borderSize);
  tft.setTextSize(params->txtSize);
  tft.setTextColor(params->txtColor, params->bgColor);
}

// Ajoute une ligne et force l'affichage immédiat de la fin (saut immédiat)
void addTFTLogLineImmediate(const char *s) {
  // copie avec limite - buffer tournant sur les lignes
  strncpy(linesBuf[bufHead], s, LINE_LEN - 1);
  linesBuf[bufHead][LINE_LEN - 1] = '\0';

  bufHead = (bufHead + 1) % MAX_LINES;  // index buffer tournant
  if (bufCount < MAX_LINES) {
    bufCount++;  // buffer pas plein on occupe une ligne de plus (topLineIndex reste à 0)
  } else {
    // buffer plein : écrase la plus ancienne -> topLineIndex avance d'une pour garder cohérence
    topLineIndex = (topLineIndex + 1) % bufCount;  // 1ère ligne affichée
  }

  drawFrameDirect();
}

// Dessine directement l'écran sans sprite : reconstruit les lignes visibles.
// Méthode simple et à faible mémoire.
void drawFrameDirect() {
  tft.fillScreen(LOG_PANEL_BG_COLOR);
  tft.setTextColor(LOG_PANEL_TEXT_COLOR, LEFT_PANEL_BG_COLOR);
  tft.setTextSize(LOG_PANEL_TEXT_SIZE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(true);

  if (bufCount == 0) return;

  int y = 0;
  for (int i = 0; i < bufCount; i++) {
    char tmp[LINE_LEN];
    int index = (topLineIndex + i) % MAX_LINES;
    strncpy(tmp, linesBuf[index], sizeof(tmp));
    tft.drawString(tmp, 2, y + 1, 1);
    y += lineHeight;
  }
}

void log_begin() {
  int i;
  // allocation dynamique des lignes du buffer de scroll
  linesBuf = (char **)malloc(MAX_LINES * sizeof(char *));
  if (!linesBuf) {
    Serial.println("Erreur allocation lignes");
    while (1)
      ;
  }

  bool pb = false;
  for (i = 0; (i < MAX_LINES) && !pb; i++) {
    linesBuf[i] = (char *)malloc(LINE_LEN * sizeof(char));
    pb = (linesBuf[i] == NULL);
  }
  if (pb) {
    Serial.printf("Erreur allocation ligne[%d]\n", i);
    while (1)
      ;
  }
}

void log_end() {
  // on libère la mémoire occupée par les lignes
  for (int i = 0; i < MAX_LINES; i++) {
    free(linesBuf[i]);
  }
  free(linesBuf);
}

void setup() {
  char buf[128];

  Serial.begin(115200);
  tft.init();
  //yield();
  tft.setRotation(3);  // 270°
  tft.fillScreen(TFT_BLACK);
  Serial.println("rotation Ok");

  delay(500);
  //yield();
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(true,true);
  tft.setTextSize(1);

  lineHeight = 10;
  Serial.println("tft setText* Ok");

  log_begin();
  Serial.println("log begin Ok");

  setVue(&vueLog);

  // exemples init
  addTFTLogLineImmediate("=== Log demarre ===");
  addTFTLogLineImmediate("Ligne 1: demarrage...");
  addTFTLogLineImmediate("Ligne 2: init OK");
  addTFTLogLineImmediate("Ligne 3: message long dépasse la largeur de l'écran (test wrapping).");

  // dessin initial
  drawFrameDirect();
  Serial.print("bufCount=");
  Serial.println(bufCount);

  Serial.print("lineHeight=");
  Serial.println(lineHeight);
  Serial.print("FreeHeap=");
  Serial.println(ESP.getFreeHeap());

  for (int i = 0; i < 30; i++) {
    delay(20);
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Log %d @ %lu ms", i, millis());
    addTFTLogLineImmediate(tmp);
  }

  char tmp[64];
  snprintf(tmp, sizeof(tmp), "============  fin du setup ========= ");
  addTFTLogLineImmediate(tmp);
  addTFTLogLineImmediate("");
  Serial.println(tmp);
  log_end();

  // attente 5s et mise en place des viewports
  delay(2000);
  tft.resetViewport();
  tft.fillScreen(TFT_BLACK);
  yield();

  setVue(&vueLeft);
  yield();
  // uint16_t old_padding = tft.getTextPadding();
  // tft.setTextPadding(3);
  tft.setCursor(2,10);
  tft.setTextSize(LEFT_PANEL_TEXT_SIZE+1);
  tft.print(" T");
  tft.setTextSize(LEFT_PANEL_TEXT_SIZE);
  tft.drawString("o", tft.getCursorX(),tft.getCursorY()-3,1);
  tft.setCursor(2, 30);
  tft.println("\n\n Relais:\n ");
  tft.print(" ");
  tft.setTextColor(TFT_BLACK, TFT_DARKGREEN);
  tft.println(" R1- ON \n");
  tft.setTextColor(LEFT_PANEL_TEXT_COLOR, LEFT_PANEL_BG_COLOR);
  tft.print(" ");
  tft.setTextColor(TFT_BLACK, TFT_RED);
  tft.println(" R2-OFF \n");
  tft.setTextColor(LEFT_PANEL_TEXT_COLOR, LEFT_PANEL_BG_COLOR);
  tft.println("");
  tft.println(" Etape:\n  ");
  tft.print("  ");
  tft.setTextColor(TFT_NAVY, TFT_WHITE);
  tft.printf(" %1d/%1d ", 1, 4);
  tft.setTextColor(LEFT_PANEL_TEXT_COLOR, LEFT_PANEL_BG_COLOR);
  // tft.setTextPadding(old_padding);

  yield();
  setVue(&vueTop);
  yield();
  tft.drawString("Four Verrier", 125, 1);
  tft.resetViewport();

  yield();

  setVue(&vueBottom);
  yield();
  tft.drawString("bottom demo panel", 25, 1);

  tft.resetViewport();
  yield();

  setVue(&vueMain);
}

double arrondi(double f, int d) {
  double mul = pow(10.0, d);
  return (round(f * mul) / mul);
}

void loop() {
  static int cpt = 0;
  if (millis() - lastAdd > 500) {
    lastAdd = millis();
    char m[8];
    snprintf(m, sizeof(m), " %3.1f", arrondi((1.0 * (cpt++) / 4.0), 1));
    yield();
    //Serial.printf("\n%s",m);
    uint8_t old_datum = tft.getTextDatum();
    //uint8_t old_font = tft.getTextFont();
    tft.setTextDatum(TR_DATUM);
    int xposValue = tft.getViewportWidth()/2 -4 ;

    tft.drawString("T four=", xposValue, 8, 2);
    //tft.fillRect(50, 50, (strlen(m) * 7), lineHeight, MAIN_PANEL_BG_COLOR);
    //int xposValue= 52+10*(strlen(m)+1);
    xposValue += 80;
    tft.drawString(m, xposValue, 8, 4);
    tft.setTextDatum(TL_DATUM);
    //tft.fillRect()
    yield();
    tft.drawString("o", xposValue + 4, 4, 1);
    tft.drawString("C", xposValue + 12, 8, 2);
    //tft.drawSpot(130, MAIN_PANEL_Y + 50, 10, MAIN_PANEL_TEXT_COLOR, MAIN_PANEL_BG_COLOR);
    //tft.drawLine(5, 5, MAIN_PANEL_X + MAIN_PANEL_WIDTH - 10, 5, TFT_RED);
    // Serial.print("Main_panel Width: ");
    // Serial.println(MAIN_PANEL_WIDTH);
    tft.setTextDatum(old_datum);
  }
}
