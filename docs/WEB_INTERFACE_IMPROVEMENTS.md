# Améliorations Interface Web - Gestionnaire de Fichiers

## 📋 Vue d'ensemble

Le fichier `data/www/files.html` a été amélioré pour offrir une meilleure gestion des mises à jour firmware via carte SD.

---

## ✨ Nouvelles Fonctionnalités

### 1. **Indicateur de Statut de Mise à Jour SD**

Affiche en temps réel si une mise à jour est en attente :

```
┌─────────────────────────────────────────────────────────┐
│ ⚠️ Mise à jour SD en attente                           │
│ Un firmware sera appliqué au prochain redémarrage       │
│ Fichier: firmware.bin                                   │
└─────────────────────────────────────────────────────────┘
```

Ou si aucune mise à jour n'est en attente :

```
┌─────────────────────────────────────────────────────────┐
│ ℹ️ Aucune mise à jour SD en attente                    │
│ Vous pouvez téléverser un nouveau firmware              │
└─────────────────────────────────────────────────────────┘
```

**Détection automatique** : Vérifie la présence de `/updates/.pending`

---

### 2. **Deux Méthodes de Mise à Jour**

#### Bouton "⚡ Appliquer OTA Immédiat"
- Applique la mise à jour **immédiatement** via WiFi
- **Risques** : Échec si signal WiFi faible ou instable
- **Usage** : Connexion WiFi excellente uniquement

#### Bouton "💾 Appliquer au Redémarrage (SD)" ⭐ **RECOMMANDÉ**
- Crée le marqueur `.pending`
- Redémarre l'ESP32
- Mise à jour appliquée **au boot** depuis la carte SD
- **Avantages** :
  - ✅ Pas de risque lié au WiFi
  - ✅ Retry automatique en cas d'échec
  - ✅ Plus sûr et fiable

---

### 3. **Messages d'Avertissement Détaillés**

#### OTA Immédiat
```
⚡ MISE À JOUR OTA IMMÉDIATE via WiFi

Fichier: firmware.bin

⚠️ RISQUES:
- Échec si signal WiFi faible
- ESP32 peut devenir inaccessible si erreur réseau

✅ RECOMMANDÉ: Utiliser "Appliquer au Redémarrage (SD)" à la place

Continuer quand même avec OTA immédiat?
[Annuler] [OK]
```

#### Mise à Jour SD
```
💾 MISE À JOUR VIA CARTE SD

Fichier: firmware.bin

✅ AVANTAGES:
- Appliquée au prochain boot (pas de risque WiFi)
- Retry automatique si échec
- Plus sûr que OTA WiFi

L'ESP32 va redémarrer maintenant et appliquer la mise à jour.

Continuer?
[Annuler] [OK]
```

---

### 4. **Validation du Nom de Fichier**

Si le fichier sélectionné n'est pas `firmware.bin` :

```
⚠️ ATTENTION: Le fichier doit s'appeler "firmware.bin"

Fichier sélectionné: my_custom_name.bin

Voulez-vous continuer quand même?
(La mise à jour échouera si le fichier n'est pas nommé firmware.bin)
[Annuler] [OK]
```

---

### 5. **Filtrage Automatique des Fichiers Système**

Les fichiers commençant par `.` (comme `.pending`) sont :
- ✅ Détectés pour l'affichage du statut
- ❌ Exclus de la liste des firmwares sélectionnables
- Évite les erreurs de sélection accidentelle

---

## 🎨 Améliorations Visuelles

### Nouvelles Classes CSS

```css
.status-box {
    padding: 12px;
    margin: 10px 0;
    border-radius: 4px;
    border-left: 4px solid;
    font-size: 0.9em;
}

.status-box.info {
    background: #E3F2FD;
    border-color: #2196F3;
    color: #0D47A1;
}

.status-box.warning {
    background: #FFF3E0;
    border-color: #FF9800;
    color: #E65100;
}

.status-box.success {
    background: #E8F5E9;
    border-color: #4CAF50;
    color: #1B5E20;
}
```

### Boutons Stylisés

- **OTA Immédiat** : Orange (`#FF9800`) - indique prudence
- **Via SD** : Vert (`#4CAF50`) - indique méthode recommandée

---

## 📝 Workflow Utilisateur Recommandé

### Scénario 1 : Mise à Jour Normale (WiFi Stable)

1. Accéder à `/files.html`
2. Section "🔧 Mise à jour Firmware"
3. Vérifier statut : "ℹ️ Aucune mise à jour SD en attente"
4. Cliquer sur "Choisir un fichier" → Sélectionner `firmware.bin`
5. Cliquer "📤 Téléverser Firmware (.bin)"
6. Attendre confirmation : "Firmware téléversé avec succès dans /updates/!"
7. **Statut change automatiquement** : "⚠️ Mise à jour SD en attente"
8. Cliquer "💾 Appliquer au Redémarrage (SD)"
9. Confirmer → ESP32 redémarre
10. Mise à jour appliquée au boot
11. Recharger la page après ~30 secondes

---

### Scénario 2 : Mise à Jour d'Urgence (WiFi Excellent)

1. Même étapes 1-7 que ci-dessus
2. Sélectionner `firmware.bin` dans la liste déroulante
3. Cliquer "⚡ Appliquer OTA Immédiat"
4. Confirmer en comprenant les risques
5. Mise à jour appliquée immédiatement
6. ESP32 redémarre après succès

---

### Scénario 3 : Mise à Jour Hors Ligne (Pas de WiFi)

1. Retirer la carte SD
2. Copier `firmware.bin` dans `/updates/`
3. Créer fichier vide `.pending` dans `/updates/`
4. Réinsérer carte SD
5. Redémarrer ESP32
6. Mise à jour appliquée au boot

---

## 🔧 Endpoints API Utilisés

### GET `/api/list?dir=/updates`
- Liste tous les fichiers dans `/updates/`
- Utilisé pour :
  - Détecter `.pending`
  - Remplir la liste déroulante des firmwares

### POST `/upload_to?dest=/updates`
- Upload firmware dans `/updates/`
- **Crée automatiquement** `/updates/.pending`

### POST `/apply_ota`
- Body: `{"path": "/updates/firmware.bin"}`
- Applique mise à jour OTA immédiate via WiFi

### POST `/apply_sd_update` ⭐ **NOUVEAU**
- Crée `/updates/.pending`
- Redémarre ESP32
- Mise à jour appliquée au boot

---

## 📊 Comparaison des Méthodes

| Critère | OTA Immédiat | SD Redémarrage |
|---------|--------------|----------------|
| **Vitesse** | ⚡ Immédiat | 🕒 +30 secondes (boot) |
| **Sécurité** | ⚠️ Moyenne (risque WiFi) | ✅ Élevée |
| **Fiabilité** | 📶 Dépend du WiFi | 💾 100% (carte SD) |
| **Retry** | ❌ Non | ✅ Automatique |
| **Rollback** | ❌ Difficile | ✅ Automatique si échec |
| **Usage recommandé** | WiFi excellent | **Défaut recommandé** |

---

## 🐛 Gestion des Erreurs

### Fichier `.pending` orphelin (firmware manquant)

**Détection** : Statut "⚠️ Mise à jour SD en attente" mais aucun `firmware.bin`

**Résolution automatique** : Au boot, si `firmware.bin` manquant :
- `.pending` supprimé
- Message de log : "ERROR: /updates/.pending exists but firmware.bin not found"
- Boot normal avec ancien firmware

---

### Mise à Jour Échoue Pendant l'Écriture

**Détection** : Écran TFT affiche "ERREUR: incomplete" ou "ERREUR: end failed"

**Résolution automatique** :
- `.pending` supprimé (évite boucle)
- Ancien firmware continue
- Utilisateur peut retry

---

## ✅ Tests Recommandés

### Test 1 : Détection du Statut
- [ ] Upload firmware → Vérifier que statut passe à "⚠️ en attente"
- [ ] Supprimer `.pending` manuellement → Vérifier que statut revient à "ℹ️ aucune"

### Test 2 : Mise à Jour SD
- [ ] Upload firmware
- [ ] Cliquer "💾 Appliquer au Redémarrage"
- [ ] Vérifier redémarrage
- [ ] Observer progression sur TFT
- [ ] Vérifier nouvelle version

### Test 3 : Filtrage Fichiers
- [ ] Créer fichier `.test` dans `/updates/`
- [ ] Vérifier qu'il n'apparaît pas dans la liste déroulante
- [ ] Vérifier que seuls les fichiers firmware sont listés

---

## 📚 Ressources

- [docs/SD_FIRMWARE_UPDATE.md](../docs/SD_FIRMWARE_UPDATE.md) - Documentation complète
- [data/updates/README.md](../data/updates/README.md) - Guide rapide dossier updates

---

**Date de mise à jour** : 2025-11-16  
**Version** : 1.1.0 (à venir)  
**Auteur** : Projet Four_Verrier
