# test/

Ce répertoire est destiné aux tests unitaires du projet avec PlatformIO Test Runner.

## Test Unitaire

Le test unitaire est une méthode de test logiciel par laquelle des unités individuelles de code source (fonctions, méthodes, classes) sont testées pour déterminer si elles fonctionnent correctement. Le test unitaire permet de détecter les problèmes tôt dans le cycle de développement.

## Structure

```
test/
├── test_main.cpp           # Tests généraux
├── test_temperature/       # Tests capteur température
│   └── test_temp.cpp
├── test_relay/             # Tests communication relais
│   └── test_relay.cpp
└── README.md               # Ce fichier
```

## Exécution des Tests

### Tous les tests
```bash
pio test -e esp32_four
```

### Tests spécifiques
```bash
pio test -e esp32_four --filter test_temperature
```

### Tests en mode verbeux
```bash
pio test -e esp32_four -v
```

## Écriture de Tests

### Exemple test basique

```cpp
// test/test_main.cpp
#include <unity.h>

void test_addition() {
    TEST_ASSERT_EQUAL(4, 2 + 2);
}

void test_temperature_range() {
    double temp = readTemperature();
    TEST_ASSERT_GREATER_OR_EQUAL(0, temp);
    TEST_ASSERT_LESS_OR_EQUAL(1400, temp);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_addition);
    RUN_TEST(test_temperature_range);
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
```

### Assertions Disponibles

```cpp
// Égalité
TEST_ASSERT_EQUAL(expected, actual);
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_EQUAL_STRING(expected, actual);

// Comparaisons
TEST_ASSERT_GREATER_THAN(threshold, actual);
TEST_ASSERT_LESS_THAN(threshold, actual);

// Booléens
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);

// Nullité
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);

// Intervalles
TEST_ASSERT_INT_WITHIN(delta, expected, actual);
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual);
```

## Configuration Tests

Dans `platformio.ini` :

```ini
[env:esp32_four]
platform = espressif32
board = esp32dev
framework = arduino

; Configuration tests
test_build_src = yes          ; Compiler src/ avec tests
test_filter = test_*          ; Pattern des tests à exécuter
test_ignore = test_hardware   ; Tests à ignorer
```

## Tests Matériels vs Simulation

### Tests Embarqués (Hardware)
Exécutés directement sur ESP32 :
```bash
pio test -e esp32_four --upload-port /dev/ttyUSB0
```

### Tests Natifs (Desktop)
Pour tests sans matériel (ajoutez env `native` dans platformio.ini) :
```bash
pio test -e native
```

## État Actuel du Projet

⚠️ **Aucun test implémenté actuellement**

### Tests Recommandés à Implémenter

1. **test_temperature/** :
   - Lecture MAX6675 valide
   - Détection erreurs capteur
   - Validation offset calibration

2. **test_relay/** :
   - Parsing protocole RelaySerial
   - Vérification commandes SSR1/SSR2
   - Gestion timeouts communication

3. **test_pid/** :
   - Calcul sortie PID
   - Limites output (0-1023)
   - Tuning parameters

4. **test_programs/** :
   - Chargement JSON programmes
   - Validation structure données
   - Exécution rampes/maintiens

5. **test_sd/** :
   - Lecture/écriture fichiers
   - Création répertoires
   - Gestion erreurs carte

6. **test_wifi/** :
   - Parsing credentials JSON
   - Fallback AP mode
   - Reconnexion auto

## Ressources

- **PlatformIO Unit Testing** : https://docs.platformio.org/en/latest/advanced/unit-testing/index.html
- **Unity Framework** : https://github.com/ThrowTheSwitch/Unity
- **Exemples Tests ESP32** : https://github.com/platformio/platform-espressif32/tree/master/examples

---

**Note** : Les tests sont essentiels pour garantir la fiabilité du contrôleur de four. Implémentez au minimum les tests critiques (température, sécurité relais) avant déploiement production.
