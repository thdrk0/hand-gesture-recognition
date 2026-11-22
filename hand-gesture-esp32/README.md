# Hand Gesture — firmware ESP32-S3 en C++

Projet indépendant de `../hand-gesture/` : la version Python reste inchangée.
La carte capture une image, détecte une main, classe son geste parmi les classes du
modèle Espressif, puis émet une sélection de **1 à 5** après stabilisation.
Tout le traitement visuel s'exécute sur la carte. Le firmware crée aussi un réseau
Wi-Fi local pour consulter la caméra et les diagnostics dans un navigateur.
Le PC sert à compiler, flasher et consulter la sortie série ou l'aperçu.

## Voir la caméra dans le navigateur

Après avoir flashé cette version, connecter le PC ou le téléphone au réseau :

- **Réseau : `HandGesture-ESP32`**
- **Mot de passe : `gesture123`**
- **Page : http://192.168.4.1** (HTTP, pas HTTPS)

Rester connecté même si Windows ou le téléphone indique « pas d'Internet ».
Le réseau est créé par la carte ; aucun identifiant de routeur n'est nécessaire.
L'image est actualisée à quelques images par seconde au maximum, suivant la durée
d'inférence et le réseau. Le bouton Pause arrête les requêtes du navigateur.

La page affiche le nombre de mains, la classe prédite (y compris `like`,
`no_gesture`, etc.), la confiance, le chiffre accepté et le chiffre stabilisé.
Un score élevé sur une classe autre que `one` à `five` ne déclenche pas de chiffre.
Les diagnostics et l'image sont lus séparément et peuvent présenter un léger décalage.
L'image affichée correspond à l'orientation réellement transmise au modèle.

La caméra n'est capturée que par la boucle de reconnaissance. Une copie JPEG
est conservée en PSRAM, puis copiée dans un tampon d'envoi : deux tampons de
128 Kio, sans nouvelle capture dans la tâche HTTP et sans verrou maintenu pendant
l'envoi réseau. Les images trop volumineuses sont ignorées ; la page signale une
image ancienne. Le Wi-Fi et le serveur ajoutent aussi une consommation de RAM
interne et peuvent augmenter la durée des cycles.

Dans le terminal ESP-IDF Windows ouvert dans `C:\esp-projects\hand-gesture-esp32` :

```powershell
idf.py -B build-windows -p COM5 build flash monitor
```

Adapter le port si Windows lui attribue un autre numéro. Quitter auparavant tout
moniteur série avec `Ctrl+]`. Le firmware n'est pas flashé automatiquement par
une simple compilation.

**Validation :** compilation ESP32-S3 réussie sous ESP-IDF 5.4.2 et tests PC
réussis. Le fonctionnement sur carte reste à vérifier ; voir [VALIDATION.md](VALIDATION.md).

## Matériel ciblé

- **ESP32-S3-EYE**, profil par défaut : caméra OV2640, au moins **8 Mo de flash et
  8 Mo de PSRAM octale**.
- Autre profil de brochage : **Seeed XIAO ESP32S3 Sense**, avec caméra DVP et les
  mêmes capacités mémoire. Ce profil doit être sélectionné dans `menuconfig`.
- Une alimentation USB stable et un câble de données sont nécessaires.

Le firmware cible exclusivement la puce **ESP32-S3**. L'ESP32-CAM AI-Thinker
classique n'est pas compatible avec ce projet : ni le brochage ni la cible des
modèles S3 ne correspondent. Un ESP32-S3 générique n'a pas un brochage caméra
universel : ajouter son profil dans `main/camera_source.cpp` avant utilisation.
Ne pas appliquer le profil S3-EYE à une autre carte simplement parce qu'elle
porte le nom « ESP32-S3 ».

## Architecture

```text
Caméra DVP → JPEG QVGA → décodage RGB888 en PSRAM
                           ↓
                HandDetect / ESPDet Pico
                           ↓ une seule main suffisamment fiable
                MobileNetV2 quantifié INT8
                           ↓ label + score
                gesture_core (C++ portable)
                           ↓ geste stable
                événement JSON sur le port série
```

Le modèle distingue notamment `one`, `two`, `three`, `four`, `five`. Les autres
classes (`like`, `ok`, etc.) ne déclenchent aucune sélection. Il s'agit d'une
**classification d'images**, différente du comptage géométrique des articulations
dans la version Python. En particulier, un pouce seul peut être classé `like`,
donc rejeté : commencer par l'index pour 1, index et majeur pour 2, index/majeur/
annulaire pour 3, quatre doigts sans le pouce pour 4, main ouverte pour 5.

Il n'y a ni MediaPipe, ni OpenCV, ni Python à exécuter sur la carte. Le backend
embarqué choisi est **ESP-DL**, avec les modèles préentraînés d'Espressif. Edge
Impulse et TensorFlow Lite Micro ne sont pas utilisés par ce firmware.

## Tester le code portable sur PC / WSL, sans carte

Prérequis : CMake et un compilateur C++17 (`g++`). Depuis ce dossier :

```bash
cmake -S host -B build-host -DCMAKE_BUILD_TYPE=Debug -DGESTURE_SANITIZERS=ON
cmake --build build-host -j2
ctest --test-dir build-host --output-on-failure
./build-host/gesture_replay host/sample.csv
```

Le programme de replay lit des observations simulées :

```csv
# time_ms,label,confidence,hand_count
0,five,0.95,1
300,five,0.96,1
600,five,0.94,1
```

Il produit :

```json
{"time_ms":600,"gesture":5,"action":"selection_5"}
```

Les lignes doivent avoir des dates en millisecondes positives ou nulles et
strictement croissantes. Le label `no_hand` avec `hand_count=0` représente
l'absence de main. Les tests exécutent le **même code de décision et de
stabilisation** que le firmware. Le replay ne capture pas la webcam et n'exécute
pas les modèles ESP-DL sur le processeur du PC : il ne mesure pas la précision
visuelle, la mémoire ou la latence réelles de la carte.

## Compiler le firmware

Version de référence : **ESP-IDF v5.4.2**. Les versions des composants directs
sont fixées dans `main/idf_component.yml` : ESP-DL 3.3.14, HandDetect 0.2.1,
HandGestureRecognition 0.2.1, esp32-camera 2.1.8 et esp_new_jpeg 1.0.2.
Le gestionnaire de composants télécharge les bibliothèques et les modèles lors
de la première configuration ; Internet est nécessaire pour cette étape.
Conserver le fichier `dependencies.lock` généré dans Git pour figer également
les dépendances transitives après une compilation réussie.

### Avec ESP-IDF installé dans WSL

Installer ESP-IDF selon le [guide officiel](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/get-started/linux-macos-setup.html),
puis activer son environnement dans le terminal :

```bash
source ~/esp/esp-idf/export.sh
cd ~/project-cv/hand-gesture-esp32
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py size
```

Dans `Hand gesture application`, choisir la carte et, si nécessaire, l'orientation
de l'image. Le projet utilise par défaut la PSRAM octale à 80 MHz : vérifier le
type de PSRAM du module avant de changer de carte. Les modèles sont intégrés dans
la partition application ; le tableau fourni réserve une application usine de
`0x7f0000` octets, sans OTA ni système de fichiers. La compilation vérifie que
l'image tient dans la partition.

### Avec Docker, sans installation locale d'ESP-IDF

Depuis ce dossier, avec Docker accessible depuis WSL :

```bash
docker run --rm -t \
  --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/project" -w /project \
  espressif/idf:v5.4.2 idf.py set-target esp32s3 build
```

Pour changer de carte, lancer d'abord la même commande avec `-it` et
`idf.py menuconfig`, puis relancer la compilation. Les fichiers générés restent
dans `build/`, `managed_components/` et `sdkconfig`.

## Flasher et utiliser la carte

Dans un terminal ESP-IDF ayant accès au port série de la carte :

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Adapter le port (`/dev/ttyUSB0`, `/dev/ttyACM0` ou `COMx` sous Windows).
Avec WSL, le périphérique USB doit être attaché à Linux pour apparaître dans
`/dev/tty*`. Autrement, utiliser ESP-IDF côté Windows pour le flash et le moniteur.
Quitter le moniteur avec `Ctrl+]`.

Présenter une seule main bien éclairée, entière dans le champ. Maintenir le geste
au moins 600 ms et trois observations consécutives. Une sélection n'est émise
qu'une fois par pose. Pour répéter le même chiffre, retirer la main ou présenter
un geste rejeté pendant la même durée, puis refaire le geste. Passer directement
à un autre chiffre stable produit aussi un événement.

La sortie série contient les événements JSON et des diagnostics. Une action
actuelle est une **sélection sur le port série**. Pour piloter un périphérique,
adapter `emit_selection()` dans `main/app_main.cpp` aux GPIO réellement disponibles
sur la carte, en conservant une fonction courte et non bloquante.

## Contraintes prises en compte

| Contrainte | Mise en œuvre |
|---|---|
| RAM limitée | Capture JPEG 320 × 240 ; un seul framebuffer en PSRAM |
| Tampon de décodage | 230 400 octets RGB888, alignés sur 16 octets, alloués une seule fois en PSRAM |
| Débordement du tampon | Dimensions et taille de sortie JPEG vérifiées avant décodage |
| Mémoire des modèles | Modèles chargés une fois par les objets ESP-DL ; poids inclus dans la flash |
| Ressources minimales | Contrôles au démarrage : PSRAM initialisée ≥ 8 Mo, flash ≥ 8 Mo |
| Bande passante | Aperçu JPEG à fréquence limitée, une seule boucle de capture, aucun encodage supplémentaire |
| Calcul | Modèles quantifiés, classification limitée à une seule main acceptée |
| Faux déclenchements | Score geste ≥ 75 %, score main ≥ 50 %, maintien 600 ms, au moins 3 observations |
| Images manquantes | Absence rejetée ; arrêt après 10 échecs consécutifs de capture |
| Interruption longue | Stabilisation réinitialisée après un intervalle > 1 500 ms |
| Watchdog | Pause entre les cycles ; watchdogs conservés |
| Diagnostic | Temps de cycle, mémoire libre, plus grand bloc PSRAM et marge de pile toutes les 5 s |

La détection, la classification et leurs intermédiaires utilisent **davantage de
mémoire** que le seul tampon RGB. ESP-DL peut effectuer des allocations internes,
notamment lors du premier appel. La consommation totale et le débit doivent être
mesurés sur le matériel ; aucune fréquence d'images n'est garantie.

Si le cycle dépasse 1 500 ms, augmenter `GESTURE_MAX_GAP_MS` dans `menuconfig` après
avoir inspecté les diagnostics. Si les gestes ne passent pas, vérifier d'abord le
profil caméra, l'éclairage et la pose avant d'abaisser le seuil de confiance.

## Fichiers

- `main/` : capture, décodage, inférence ESP-DL et sortie série.
- `components/gesture_core/` : logique C++17 commune au PC et à l'ESP32.
- `host/` : tests et replay d'observations sur PC.
- `sdkconfig.defaults`, `partitions.csv` : paramètres mémoire et flash.

Les artefacts compilés, environnements générés et modèles téléchargés ne sont pas
destinés à être copiés dans Git. Conserver les sources, les paramètres par défaut,
les tests et cette documentation. Le `sdkconfig` local dépend du matériel choisi.

## Sources techniques

- [Exemple officiel ESP-DL de reconnaissance de gestes](https://github.com/espressif/esp-dl/tree/master/examples/hand_gesture_recognition)
- [Modèle et API HandGestureRecognition](https://github.com/espressif/esp-dl/tree/master/models/hand_gesture_recognition)
- [Pilote caméra Espressif et contraintes JPEG/PSRAM](https://github.com/espressif/esp32-camera)
- [Brochages caméra de référence](https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h)
