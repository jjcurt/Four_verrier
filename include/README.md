# include/

Ce répertoire contient les fichiers d'en-tête (headers) du projet.

## Fichiers de Configuration

### Matériel et Pins
- **`pins.h`** : Définitions des broches ESP32
  - MAX6675 thermocouple (CS=27, SO=35, SCK=22)
  - SD card (CS=5)
  - Pins série UART0 (TX=1, RX=3) pour communication Nano
  - Backlight TFT

### Configuration Système
- **`config.h`** : Constantes générales
  - Plages de température (MIN_TEMP, MAX_TEMP)
  - Intervalles de mise à jour
  - Port HTTP serveur web
  - Identifiants WiFi AP/STA par défaut

### Types de Données
- **`furnace_types.h`** : Structures de données
  - `ProgramStep` : étape de programme (targetTemp, rampRate, holdTime, withRamp)
  - `FiringProgram` : programme complet (name, steps[10], numSteps, enableDataLog)

### Gestionnaires
- **`wifi_manager.h`** : Fonctions WiFi
  - `loadWiFiConfig()` : chargement credentials depuis SD
  - `saveWiFiConfig()` : sauvegarde credentials sur SD

- **`sd_manager.h`** : Utilitaires carte SD
  - Gestion fichiers programmes/logs
  - Structure `/config`, `/programs`, `/logs`, `/www`, `/updates`

- **`temperature_sensor.h`** : Interface MAX6675
  - Lecture thermocouple
  - Gestion offset calibration

- **`shared_serial.h`** : Communication série partagée
  - Macros debug conditionnelles
  - Gestion UART0 (Nano relay bridge)

### Display
- **`User_Setup.h`** : Configuration TFT_eSPI
  - Pilote ILI9341_2_DRIVER
  - Résolution 320x240 landscape
  - Pins SPI dédiées
  - Polices chargées (GLCD, FONT2, FONT4, FONT6, FONT7, GFXFF)
  - Fréquences SPI (55MHz display, 2.5MHz touch)

## Conventions

Les fichiers d'en-tête permettent de :
- Partager des déclarations C entre plusieurs fichiers source
- Centraliser les configurations matérielles
- Définir les structures de données communes
- Éviter la duplication de code

Pour inclure un header dans votre code :
```cpp
#include "pins.h"
#include "config.h"
```

## Modification User_Setup.h

⚠️ **Important** : Le fichier `User_Setup.h` est spécifique à l'ESP32-2432S028R (CYD). Ne pas utiliser les configurations par défaut de TFT_eSPI trouvées en ligne.

Points clés de configuration :
- Pilote : `ILI9341_2_DRIVER`
- Inversion display : activée (`tft.invertDisplay(true)`)
- Ordre couleurs : RGB
- Rotation : 3 (paysage, USB à gauche)
- Backlight : GPIO 21

---

Pour plus d'informations sur les headers C/C++, consultez :
https://gcc.gnu.org/onlinedocs/cpp/Header-Files.html
