# Reconnaissance de chiffres avec la webcam

Prototype Python qui reconnaît le nombre de doigts levés (1 à 5), affiche les articulations et déclenche une action de sélection dans la console. Aucun ESP32 ni jeu de données à préparer.

## Installation sur Windows (recommandée pour la webcam)

Utiliser Python 3.10 à 3.13, 64 bits. Ouvrir PowerShell dans ce dossier (`hand-gesture`), puis :

```powershell
python -m venv .venv-windows
.\.venv-windows\Scripts\python.exe -m pip install -r requirements.txt
.\.venv-windows\Scripts\python.exe app.py
```

Le projet est dans WSL mais peut être ouvert dans l'Explorateur avec le chemin `\\wsl.localhost\Ubuntu\home\tomubt\project-cv\hand-gesture`. Exécuter Python **Windows** depuis ce dossier pour accéder directement à la webcam Windows. Ne pas partager un environnement virtuel entre Windows et Linux.

## Linux avec webcam accessible

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python app.py
```

Le modèle officiel MediaPipe est téléchargé dans `models/` au premier lancement : une connexion Internet est nécessaire à cette étape. Les images sont ensuite traitées localement, sans enregistrement vidéo.

## Utilisation

- Présenter une seule main, paume vers la caméra, dans une zone éclairée.
- Lever de 1 à 5 doigts, en gardant les doigts repliés bien fermés. Pour commencer : index ; index et majeur ; index, majeur et annulaire ; quatre doigts sans le pouce ; main ouverte.
- Tenir le geste environ une demi-seconde. Le chiffre apparaît et produit une action `selection_1` à `selection_5`. Une même pose maintenue ne répète pas l'action.
- Fermer la main ou la retirer pendant une demi-seconde pour pouvoir redéclencher le même chiffre. Passer à un autre chiffre déclenche aussi une nouvelle action après stabilisation.
- Quitter avec **Q**, **Échap**, ou la croix de la fenêtre.

```powershell
.\.venv-windows\Scripts\python.exe app.py --camera 1 --hold 0.6 --events events.jsonl
```

`--camera` choisit la webcam ; `--hold` règle la durée de stabilisation ; `--events` conserve uniquement les événements dans un journal JSONL. Une action est actuellement une sélection affichée et journalisée ; aucune commande système n'est exécutée.

## Fonctionnement et limites

OpenCV capture et affiche les images. Le modèle préentraîné MediaPipe Hand Landmarker fournit 21 articulations. Des règles géométriques sur les angles et distances estiment les doigts tendus, indépendamment du choix main gauche/main droite. Un filtre temporel confirme les changements. Le chiffre désigne le **nombre de doigts levés**, pas une langue des signes normalisée.

La précision dépend de la visibilité des doigts et du pouce : occlusions, main de profil et doigts partiellement pliés peuvent donner un résultat incorrect. Ce prototype n'a pas encore de mesure de précision sur un jeu de données réel. Les tests automatiques vérifient la géométrie élémentaire et la stabilisation, pas la qualité de reconnaissance visuelle.

Si la caméra ne s'ouvre pas : fermer les applications qui l'utilisent, autoriser l'accès caméra aux applications de bureau dans Windows, puis essayer `--camera 1`. WSL n'expose généralement pas directement la webcam Windows ; utiliser les commandes Windows ci-dessus.

## Vérification

```bash
python -m unittest discover -v
```

## Évolution vers ESP32-CAM

Cette version utilise Python, OpenCV et MediaPipe sur ordinateur. Elle n'utilise pas encore Edge Impulse ou TensorFlow Lite Micro et ne se déploie pas telle quelle sur ESP32. Une première évolution possible est d'utiliser l'ESP32-CAM comme source vidéo et de conserver le traitement sur PC. Pour une inférence embarquée, il faudra collecter des images annotées, entraîner un modèle compact (par exemple avec Edge Impulse), puis mesurer sa mémoire, sa latence et sa précision sur la carte choisie.

Documentation du détecteur : https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker/python
