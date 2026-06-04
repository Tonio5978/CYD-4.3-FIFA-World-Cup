# FIFA World Cup 2026 – Live Score Display

Afficheur de scores en direct pour la **Coupe du Monde FIFA 2026**, tournant sur le **Sunton ESP32-8048S043C** (écran IPS 4.3" 800×480).

---

## Aperçu

| Écran | Description |
|-------|-------------|
| **Splash** | Démarrage avec spinner animé et statuts WiFi / NTP / API |
| **LIVE** | Cartes de matchs en cours : score, buteurs, chrono, stade |
| **Prochains matchs** | Calendrier des matchs à venir avec horaires |
| **Groupe (A–P)** | Classement (gauche) + liste des matchs du groupe (droite) |
| **Popup but** | Célébration animée : drapeau géant, buteur, score — 10 secondes |

---

## Hardware

| Caractéristique | Valeur |
|-----------------|--------|
| **Carte** | Sunton ESP32-8048S043C |
| **Processeur** | ESP32-S3 Dual-Core 240 MHz |
| **Flash** | 16 MB |
| **PSRAM** | 8 MB (Octal SPI) |
| **Écran** | 4.3" IPS 800×480, interface RGB 16-bit |
| **Touch** | Capacitif GT911 (5 points, I2C) |
| **WiFi** | 802.11 b/g/n 2.4 GHz |
| **Bluetooth** | BLE 5.0 |
| **Carte SD** | Oui (incluse 16 GB) |
| **Audio** | Amplificateur I2S MA98357 3.2 W |
| **Alimentation** | 5 V / 2 A via USB-C |

---

## Fonctionnalités

### Écran principal — Matchs LIVE

- Drapeaux des deux équipes (64×48 px)
- Score en très grand format (couleur or)
- Liste des buteurs avec la minute : `⚽ Neymar 23'`, `⚽ Vinícius Jr 45'+2`
  - Maximum 3 buteurs affichés par équipe
  - Mention `(c.s.c.)` pour les buts contre son camp
- Chrono clignotant + période (1ère MT / 2ème MT / Prolongations)
- Indicateur rouge clignotant ● LIVE
- Stade et ville

### Popup But — Goal Celebration

Déclenchée automatiquement dès qu'un nouveau but est détecté dans l'API :

```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│              ⚽ GOOOOAAAAAL ! ⚽                         │
│                                                          │
│              [ DRAPEAU 200×150 ]                         │
│                  🇧🇷                                     │
│                                                          │
│               NEYMAR JR                                  │
│                 67'                                      │
│                                                          │
│     🇧🇷 BRÉSIL  2 - 1  ARGENTINE 🇦🇷                   │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

| Paramètre | Valeur |
|-----------|--------|
| Taille popup | 600×400 px, centré sur 800×480 |
| Fond | Dégradé vertical aux couleurs de l'équipe |
| Bordure | 3 px or (#FFD700) |
| Durée | **10 secondes** |
| Animation entrée | Scale 0.5 → 1.0 + fade in (500 ms) |
| Animation sortie | Fade out (300 ms) |
| Gestion multi-buts | File d'attente — les popups s'affichent les unes après les autres |
| But contre son camp | Drapeau de l'équipe bénéficiaire + mention `(c.s.c.)` |

### Écran Groupe

- **Gauche (400 px)** : Tableau de classement
  - Rang, drapeau, code équipe, Pts, J, V, N, D
  - Fond vert pour les 2 équipes qualifiées
  - Buts pour / Buts contre / Différence de buts
- **Droite (360 px)** : Matchs du groupe
  - Score ou horaire selon l'état du match
  - Indicateur LIVE pour les matchs en cours
- Navigation par touch (flèches et boutons groupes en footer)

---

## Source de données — ESPN API

| API | URL |
|-----|-----|
| **Scores en direct** | `https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard?limit=950&dates=20260611-20260720` |
| **Classements** | `https://site.web.api.espn.com/apis/v2/sports/soccer/fifa.world/standings` |

| Condition | Intervalle |
|-----------|------------|
| Match en cours | 10 secondes |
| Aucun match | 5 minutes |
| Classements | 10 minutes |
| Sync NTP | 1 heure |

---

## Calendrier FIFA 2026

| Date | Étape |
|------|-------|
| **11 juin 2026** | Match d'ouverture |
| 12 – 26 juin | Phase de groupes (16 groupes × 3 matchs) |
| 28 juin – 3 juillet | 8èmes de finale |
| 5 – 7 juillet | Quarts de finale |
| 9 – 10 juillet | Demi-finales |
| 13 juillet | Match pour la 3ème place |
| **19 juillet 2026** | **FINALE** |

**Format** : 48 équipes, 16 groupes de 3, 2 qualifiés par groupe → 104 matchs au total

---

## Installation

### 1. Prérequis logiciels

- [PlatformIO](https://platformio.org/) (CLI ou extension VS Code)
- Python 3.8+ avec Pillow (pour le script de téléchargement des drapeaux)

### 2. Configuration WiFi

Éditer `include/config.h` :

```cpp
#define WIFI_SSID     "VotreSSID"
#define WIFI_PASSWORD "VotreMotDePasse"
```

### 3. Préparer les drapeaux sur la carte SD

#### Structure attendue sur la carte SD

```
SD:/
└── flags/
    ├── ARG.png          ← 64×48 px  (cartes matchs + tableaux groupes)
    ├── BRA.png
    ├── FRA.png
    ├── … (48 fichiers)
    └── large/
        ├── ARG.png      ← 200×150 px  (popup but uniquement)
        ├── BRA.png
        ├── FRA.png
        └── … (48 fichiers)
```

> **Important :** Le dossier `flags/` doit être **à la racine** de la carte SD, pas dans un sous-dossier.

#### Méthode 1 — Script automatique (recommandé)

Le script `tools/fetch_flags.py` télécharge les drapeaux depuis [flagcdn.com](https://flagcdn.com) (gratuit, sans clé API) et les redimensionne aux deux tailles nécessaires.

**Prérequis :**
```bash
pip install Pillow
```

**Télécharger tous les 48 drapeaux :**
```bash
python tools/fetch_flags.py
```

**Options utiles :**
```bash
python tools/fetch_flags.py --team BRA          # un seul pays
python tools/fetch_flags.py --only-small        # uniquement 64×48 (si large déjà faits)
python tools/fetch_flags.py --only-large        # uniquement 200×150 (si small déjà faits)
python tools/fetch_flags.py --skip-existing     # ne re-télécharge pas si le fichier existe
```

Les fichiers sont générés dans `data/flags/` (chemin relatif au projet).

**Copier sur la carte SD :**
```
data/flags/  →  SD:/flags/
```

Copier **tout** le dossier `data/flags/` (avec le sous-dossier `large/`) à la racine de la carte SD.

---

#### Méthode 2 — Manuellement

Si vous préférez utiliser vos propres images :

1. Préparer une image PNG pour chaque équipe
2. Créer deux versions redimensionnées :
   - **64×48 px** — enregistrer sous `SD:/flags/<CODE>.png`
   - **200×150 px** — enregistrer sous `SD:/flags/large/<CODE>.png`
3. Le nom du fichier doit correspondre **exactement** au code abrégé ESPN (voir tableau ci-dessous)

> **Outils gratuits :** [ezgif.com/resize](https://ezgif.com/resize), GIMP, Paint.NET, Photoshop.
> Utiliser du remplissage noir (letterbox) pour respecter le ratio 4:3.

---

#### Codes équipes FIFA 2026

Le nom du fichier PNG doit correspondre au code renvoyé par l'API ESPN.
Les codes utilisés par le firmware sont les suivants :

| Confédération | Code | Pays |
|---------------|------|------|
| **CONMEBOL** | `BRA` | Brésil |
| | `ARG` | Argentine |
| | `URU` | Uruguay |
| | `COL` | Colombie |
| | `CHI` | Chili |
| | `PER` | Pérou |
| **UEFA** | `FRA` | France |
| | `GER` | Allemagne |
| | `ESP` | Espagne |
| | `ENG` | Angleterre |
| | `ITA` | Italie |
| | `NED` | Pays-Bas |
| | `POR` | Portugal |
| | `BEL` | Belgique |
| | `CRO` | Croatie |
| | `SUI` | Suisse |
| | `POL` | Pologne |
| | `DEN` | Danemark |
| | `SWE` | Suède |
| | `UKR` | Ukraine |
| | `AUT` | Autriche |
| | `CZE` | République Tchèque |
| **CAF** | `MAR` | Maroc |
| | `SEN` | Sénégal |
| | `TUN` | Tunisie |
| | `NGR` | Nigeria |
| | `CMR` | Cameroun |
| | `EGY` | Égypte |
| | `GHA` | Ghana |
| | `CIV` | Côte d'Ivoire |
| | `ALG` | Algérie |
| **AFC** | `JPN` | Japon |
| | `KOR` | Corée du Sud |
| | `IRN` | Iran |
| | `AUS` | Australie |
| | `SAU` | Arabie Saoudite |
| | `QAT` | Qatar |
| | `IRQ` | Irak |
| | `UAE` | Émirats Arabes Unis |
| **CONCACAF** | `USA` | États-Unis |
| | `MEX` | Mexique |
| | `CAN` | Canada |
| | `CRC` | Costa Rica |
| | `JAM` | Jamaïque |
| | `HON` | Honduras |
| | `PAN` | Panama |
| | `TRI` | Trinité-et-Tobago |
| **OFC** | `NZL` | Nouvelle-Zélande |

> **Note :** L'API ESPN peut utiliser des abréviations légèrement différentes pour certaines équipes.
> En cas de drapeau non affiché, brancher le câble USB et lancer `pio device monitor` :
> la ligne `[SD] Flag not found: /flags/XXX.png` indique le code exact attendu.

### 4. Flasher le firmware

```bash
pio run --target upload
pio device monitor
```

### 5. Démarrage

1. Insérer la carte SD (avec les drapeaux)
2. Alimenter via USB-C (5 V / 2 A)
3. Attendre la connexion WiFi + sync NTP (~5 s)
4. L'écran principal s'affiche automatiquement

---

## Structure du projet

```
CYD-4.3-FIFA-World-Cup/
├── platformio.ini          Configuration PlatformIO (ESP32-S3, 16 MB)
├── include/
│   ├── config.h            Tous les #define : WiFi, API, couleurs, pins
│   └── screens.h           Structs partagés : Match, Goal, GoalPopup, AppContext
├── src/
│   ├── main.cpp            setup / loop — machine d'état + polling API
│   ├── display_config.h    Classe LGFX (LovyanGFX, RGB parallel + GT911)
│   ├── wifi_manager        Connexion + reconnexion automatique
│   ├── ntp_time            Sync horaire, formatage français
│   ├── storage             Montage SD, chargement PNG → RGB565
│   ├── espn_api            Fetch API ESPN, parsing JSON, détection buts
│   ├── ui_manager          Cache drapeaux PSRAM (LRU), header/footer, popup
│   ├── screen_splash       Écran de démarrage animé
│   ├── screen_home         Matchs LIVE ou prochains matchs
│   └── screen_group        Classement + matchs d'un groupe
├── tools/
│   └── fetch_flags.py      Script Python : télécharge et redimensionne les drapeaux
└── data/
    ├── flags/              → copier sur la carte SD
    │   ├── BRA.png         64×48 px
    │   └── large/
    │       └── BRA.png     200×150 px
    └── sounds/
        └── goal_sound.wav  (optionnel — 16 kHz, mono, WAV)
```

---

## Dépendances

| Librairie | Version | Usage |
|-----------|---------|-------|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | ^1.1.16 | Driver écran RGB + touch GT911 |
| [ArduinoJson](https://arduinojson.org/) | ^7.0.4 | Parsing JSON ESPN API |
| [PNGdec](https://github.com/bitbank2/PNGdec) | ^1.0.1 | Décodage PNG drapeaux depuis SD |

---

## Options de configuration (`include/config.h`)

```cpp
// Popup but
#define ENABLE_GOAL_POPUP        true
#define GOAL_POPUP_DURATION_MS   10000   // durée affichage (ms)
#define GOAL_POPUP_ANIM_IN_MS    500     // animation entrée (ms)
#define GOAL_POPUP_ANIM_OUT_MS   300     // animation sortie (ms)

// Son (nécessite haut-parleur sur I2S)
#define ENABLE_GOAL_SOUND        false

// Timezone
#define NTP_TIMEZONE  "CET-1CEST,M3.5.0,M10.5.0/3"  // Europe/Paris
```

---

## Dépannage

| Problème | Solution |
|----------|----------|
| Écran blanc au démarrage | Vérifier les pins RGB dans `src/display_config.h` |
| Touch ne répond pas | Vérifier `TOUCH_SDA_PIN`, `TOUCH_SCL_PIN`, `TOUCH_INT_PIN`, `TOUCH_RST_PIN` dans `config.h` (varient selon révision PCB) |
| Drapeaux non affichés | Vérifier que `flags/` est à la racine de la SD ; vérifier `SD_CS_PIN` |
| Pas de données ESPN | Vérifier la connexion WiFi ; l'API ESPN ne nécessite pas de clé |
| Freeze pendant la popup | Réduire `CACHE_SIZE` dans `ui_manager.cpp` si PSRAM insuffisante |
