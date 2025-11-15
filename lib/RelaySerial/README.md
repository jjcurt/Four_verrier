# RelaySerial Library

Bibliothèque de communication série pour contrôle des relais SSR entre ESP32 et Arduino Nano.

## Description

Cette bibliothèque implémente un protocole série simple pour commander deux relais statiques (SSR) via un Arduino Nano qui fait office de pont série. L'ESP32 envoie des commandes par UART0 et le Nano traduit ces commandes en signaux de contrôle pour les SSRs.

## Architecture

```
ESP32 (main controller)
    │
    │ UART0 (115200 baud)
    │ TX (GPIO1) → RX Nano
    │ RX (GPIO3) ← TX Nano
    │
    ▼
Arduino Nano (relay bridge)
    │
    ├─→ SSR1 (Digital ON/OFF)
    └─→ SSR2 (PWM 0-255)
```

## Protocole Série

### Paramètres UART
- **Baud rate** : 115200
- **Data bits** : 8
- **Parity** : None
- **Stop bits** : 1
- **Flow control** : None

### Format des Commandes

**Commande SSR** :
```
SSR1:<valeur>,SSR2:<valeur>\n
```

- **SSR1** : Relais principal (sécurité)
  - Type : ON/OFF tout ou rien
  - Valeurs acceptées : `0` (OFF) ou `1` (ON)
  - Usage : Coupe complètement l'alimentation si OFF
  
- **SSR2** : Relais de modulation
  - Type : PWM (Pulse Width Modulation)
  - Valeurs acceptées : `0-1023` (10 bits côté ESP32)
  - Conversion Nano : mapped sur 0-255 (8 bits PWM Arduino)
  - Usage : Contrôle fin de la puissance (PID)

**Exemples** :
```
SSR1:1,SSR2:512\n    # SSR1 ON, SSR2 à 50% (512/1023)
SSR1:0,SSR2:0\n      # Tout arrêté
SSR1:1,SSR2:1023\n   # Puissance maximale
```

### Requête d'État (optionnel)

**Commande** :
```
GET:SSR1\n
GET:SSR2\n
```

**Réponse** :
```
SSR1:1\n
SSR2:512\n
```

## Utilisation (côté ESP32)

### Initialisation

```cpp
#include <relay_serial.h>

void setup() {
    // Initialiser communication série avec Nano
    RelaySerial::begin(115200);
    
    // Demander état initial (optionnel)
    RelaySerial::requestState(1);
    RelaySerial::requestState(2);
}
```

### Commande des Relais

```cpp
// SSR1 : ON/OFF tout ou rien
RelaySerial::setSSR(1, true);   // Allumer SSR1
RelaySerial::setSSR(1, false);  // Éteindre SSR1

// SSR2 : PWM 0-1023 (10 bits)
RelaySerial::setSSR(2, 0);      // 0% (arrêt)
RelaySerial::setSSR(2, 256);    // ~25%
RelaySerial::setSSR(2, 512);    // 50%
RelaySerial::setSSR(2, 768);    // ~75%
RelaySerial::setSSR(2, 1023);   // 100% (pleine puissance)
```

### Exemple Complet

```cpp
#include <relay_serial.h>

double pidOutput = 0;  // Sortie PID (0-1023)
bool programRunning = false;

void loop() {
    // Contrôle SSR1 selon état programme
    RelaySerial::setSSR(1, programRunning);
    
    // Contrôle SSR2 selon sortie PID (modulation fine)
    if (programRunning) {
        RelaySerial::setSSR(2, (uint16_t)pidOutput);
    } else {
        RelaySerial::setSSR(2, 0);  // Arrêt si programme non actif
    }
}
```

## Configuration (côté Arduino Nano)

Le sketch Nano doit :
1. Écouter sur Serial (115200 baud)
2. Parser les commandes `SSR1:<val>,SSR2:<val>\n`
3. Convertir SSR2 de 10 bits (0-1023) vers 8 bits (0-255) : `map(val, 0, 1023, 0, 255)`
4. Appliquer les valeurs sur les pins de sortie

**Exemple sketch Nano** : Voir `tools/nano_pio/src/relay_nano.ino`

## Sécurité

### Architecture Redondante
- **SSR1** et **SSR2** montés en **série** électriquement
- Si SSR1 est OFF, aucun courant ne passe même si SSR2 est actif
- Double coupure pour sécurité maximale

### Gestion des Erreurs
```cpp
// Toujours arrêter SSR2 avant SSR1 (séquence sûre)
RelaySerial::setSSR(2, 0);    // D'abord couper modulation
delay(100);                    // Attendre stabilisation
RelaySerial::setSSR(1, false); // Puis couper alimentation
```

## Debug

### Activation Debug
Dans `platformio.ini` ou `library.json` :
```ini
build_flags = -DRELAY_DEBUG
```

### Sortie Debug
```
[RELAY] TX: SSR1:1,SSR2:512
[RELAY] RX: SSR1:1
[RELAY] RX: SSR2:512
```

## Dépendances

- **Hardware Serial (UART0)** : `Serial` object (ESP32)
- **Macros pins** : `relay_pins.h` (RELAY_TX_PIN, RELAY_RX_PIN)

⚠️ **Attention** : UART0 est partagé avec USB sur ESP32. En production, `DEBUG_SERIAL` doit être désactivé pour éviter conflits.

## Compatibilité

- **ESP32** : UART0 (GPIO1/GPIO3)
- **Arduino Nano** : Serial (pins RX=0, TX=1)
- **Baud rates testés** : 9600, 57600, 115200 (115200 recommandé)

## Structure Fichiers

```
lib/RelaySerial/
├── src/
│   ├── relay_serial.cpp      # Implémentation
│   └── relay_serial.h        # Header privé
├── include/
│   ├── relay_serial.h        # Header public (API)
│   └── relay_pins.h          # Définitions pins
├── library.json              # Métadonnées PlatformIO
└── README.md                 # Ce fichier
```

## Licence

Propriétaire - Voir LICENSE à la racine du projet

---

**Version** : 1.0.0  
**Auteur** : Projet Four Verrier  
**Date** : 15 novembre 2025
