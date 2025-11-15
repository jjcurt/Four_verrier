# data/

Ce répertoire contient les fichiers de données qui seront copiés sur la carte SD pour le fonctionnement du contrôleur de four.

## Structure SD Card

La carte SD doit avoir l'arborescence suivante :

```
/
├── config/
│   ├── wifi.json           # Identifiants WiFi (créé manuellement)
│   └── settings.json       # Paramètres système (auto-généré)
├── programs/
│   ├── bisque.json         # Programmes de cuisson
│   ├── raku.json
│   └── test.json
├── logs/
│   └── *.csv               # Logs CSV (auto-générés)
├── www/
│   ├── index.html          # Interface web
│   ├── control.html
│   ├── files.html
│   └── styles.css
└── updates/
    └── *.bin               # Firmwares OTA (temporaires)
```

## Répertoires Détaillés

### `/config` - Configuration Système

#### `wifi.json` (obligatoire, à créer manuellement)
Identifiants réseau WiFi. Utilisez le template fourni :

```bash
cp data/config/wifi.json.example data/config/wifi.json
# Puis éditez avec vos credentials
```

**Format** :
```json
{
  "ssid": "VotreReseauWiFi",
  "password": "VotreMotDePasse"
}
```

⚠️ **Sécurité** : Ce fichier contient des credentials en clair. Ne jamais le commiter sur GitHub (déjà dans `.gitignore`).

#### `settings.json` (auto-généré)
Paramètres PID et configuration système. Créé automatiquement au premier démarrage.

**Format** :
```json
{
  "updateInterval": 1000,
  "pidKp": 40.0,
  "pidKi": 0.1,
  "pidKd": 4.0,
  "tempOffset": 0.0
}
```

**Modification** : Via interface web (`/control.html`) ou directement sur SD card.

---

### `/programs` - Programmes de Cuisson

Fichiers JSON définissant les cycles de cuisson. Un programme contient plusieurs étapes avec rampes et maintiens.

**Format** :
```json
{
  "name": "Bisque Cone 04",
  "numSteps": 3,
  "enableDataLog": true,
  "steps": [
    {
      "targetTemp": 200,
      "rampRate": 50,
      "holdTime": 30,
      "withRamp": true
    },
    {
      "targetTemp": 600,
      "rampRate": 100,
      "holdTime": 60,
      "withRamp": true
    },
    {
      "targetTemp": 1060,
      "rampRate": 150,
      "holdTime": 20,
      "withRamp": true
    }
  ]
}
```

**Paramètres d'étape** :
- `targetTemp` : Température cible (°C)
- `rampRate` : Vitesse de montée (°C/min)
- `holdTime` : Durée de maintien (minutes)
- `withRamp` : `true` = rampe progressive, `false` = saut direct

**Paramètres programme** :
- `name` : Nom affiché sur interface
- `numSteps` : Nombre d'étapes (1-10)
- `enableDataLog` : `true` = génère CSV dans `/logs`

**Gestion via web** :
- Upload : `/files.html` → onglet "Programs"
- Téléchargement : bouton "Download"
- Suppression : bouton "Delete"

---

### `/logs` - Journaux CSV

Fichiers générés automatiquement quand `enableDataLog: true` dans un programme.

**Nom fichier** : `<nom_programme>_YYYYMMDD_HHMMSS.csv`

**Exemple** : `bisque_cone_04_20251115_143022.csv`

**Colonnes** :
```csv
Timestamp,ElapsedSeconds,CurrentTemp,RawTemp,TargetTemp,PidOutput,SSR2_PWM,Phase,Step
2025-11-15 14:30:22,0.0,25.50,25.00,200.00,0.00,0,RAMP,1
2025-11-15 14:30:27,5.0,26.30,25.80,200.00,512.00,512,RAMP,1
2025-11-15 14:30:32,10.0,27.10,26.60,200.00,723.00,723,RAMP,1
```

**Utilisation** :
- Analyse PID tuning (Excel, Python, MATLAB)
- Traçage courbes température
- Debug programmes de cuisson

**Téléchargement** : `/files.html` → onglet "Logs" → bouton "Download"

---

### `/www` - Interface Web

Fichiers HTML/CSS/JS servis par le serveur web embarqué.

**Pages principales** :
- **`index.html`** : Page d'accueil, navigation
- **`control.html`** : Contrôle four (démarrage programmes, PID, monitoring)
- **`files.html`** : Gestionnaire fichiers (upload/download/delete)

**Modification** :
1. Éditer localement dans `data/www/`
2. Copier sur SD card
3. OU upload via `/files.html` → onglet "Updates" → `dest=/www`

**URLs** :
- Accueil : `http://<IP>/`
- Contrôle : `http://<IP>/control.html`
- Fichiers : `http://<IP>/files.html`
- WiFi config : `http://<IP>/wifi`

---

### `/updates` - Mises à Jour OTA

Répertoire temporaire pour firmwares OTA (Over-The-Air).

**Workflow** :
1. Compiler firmware : `pio run -e esp32_four`
2. Upload via web : `/files.html` → onglet "Updates" → upload `.bin`
3. Appliquer : clic sur "Apply OTA" → reboot automatique
4. Nettoyage : `.bin` supprimés automatiquement au boot suivant

**Sécurité** :
- Vérification taille avant application
- Flush progressif (évite overflow RAM)
- Suppression automatique après succès

---

## Préparation Carte SD

### Formatage
- **Système de fichiers** : FAT32 (recommandé)
- **Taille allocation** : 4096 bytes (par défaut)
- **Classe** : Classe 10 (lecture/écriture rapide)

### Copie Initiale

```bash
# Depuis la racine du projet
cp -r data/www /media/SD_CARD/
cp data/config/wifi.json.example /media/SD_CARD/config/wifi.json
# Éditez wifi.json avec vos credentials
```

### Vérification Structure

Sur la SD card, vérifier présence de :
```
/config/wifi.json
/programs/test.json
/www/index.html
/www/control.html
/www/files.html
```

Les dossiers `/logs` et `/updates` seront créés automatiquement au premier boot.

---

## Dépannage

### WiFi ne se connecte pas
1. Vérifier `/config/wifi.json` présent et valide (JSON syntaxe correcte)
2. Vérifier SSID/password exacts (sensible à la casse)
3. Fallback AP si échec : SSID `FurnaceController` (voir `config.h`)

### Programme ne charge pas
1. Vérifier syntaxe JSON (https://jsonlint.com/)
2. Vérifier `numSteps` correspond au nombre d'éléments dans `steps[]`
3. Vérifier extension `.json`

### Logs CSV non générés
1. Vérifier `enableDataLog: true` dans le programme JSON
2. Vérifier espace libre sur SD (>100 MB recommandé)
3. Vérifier répertoire `/logs` créé (auto-créé normalement)

### Interface web inaccessible
1. Vérifier IP affiché sur écran TFT au démarrage
2. Vérifier fichiers dans `/www` (index.html, control.html, files.html)
3. Tester avec navigateur moderne (Chrome, Firefox, Safari)

---

## Capacité SD Card

**Recommandations** :
- **Minimum** : 512 MB
- **Recommandé** : 2-8 GB
- **Maximum testé** : 32 GB (FAT32)

**Estimation espace** :
- Interface web (`/www`) : ~50 KB
- Programme JSON : ~1 KB chacun
- Log CSV : ~5 KB par heure de cuisson
- Firmware OTA : ~1 MB temporaire

---

**Version** : 1.0.0  
**Date** : 15 novembre 2025
