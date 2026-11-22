# Validation initiale (avant ajout de l'aperçu Wi-Fi)

L'aperçu Wi-Fi ajouté ultérieurement compile également sous ESP-IDF 5.4.2 pour
le profil XIAO Sense sous Windows : binaire de 3 221 760 octets, 61 % de partition
libre. La syntaxe JavaScript de la page a été vérifiée avec Node.js.
Il doit être vérifié sur carte : connexion au
point d'accès, ouverture de la page, image orientée correctement, diagnostics et
maintien de la reconnaissance. Les mesures ci-dessous décrivent la version initiale.

## Tests sur PC

- Compilation C++17 avec GCC 11.4, avertissements traités comme erreurs.
- AddressSanitizer et UndefinedBehaviorSanitizer activés.
- 42 vérifications : classes 1 à 5, seuil de confiance, absence de main,
  plusieurs mains, valeurs invalides, maintien, relâchement, répétition,
  interruption de capture et retour en arrière de l'horloge.
- Replay vérifié automatiquement : sélections `1`, `1`, `4`, `5` aux dates
  attendues, sans événement supplémentaire pendant une pose maintenue.
- Résultat : **2 tests CTest sur 2 réussis**.

## Compilation du firmware

- ESP-IDF **v5.4.2**, compilateur Xtensa GCC **14.2.0**.
- Cible : **ESP32-S3**, profil par défaut **ESP32-S3-EYE**.
- Dépendances et modèles réels téléchargés via le gestionnaire de composants ;
  versions transitives consignées dans `dependencies.lock`.
- Binaire application généré : `build/hand_gesture_esp32.bin`.
- Taille du binaire application : **2 661 840 octets**, environ **2,54 Mio**.
- Partition application : `0x7f0000` octets ; **68 % libres**.
- Chargeur de démarrage également compilé et contrôle de partition réussi.

## Vérifications restant à faire sur carte

Aucun ESP32 n'était connecté pendant cette validation. Le firmware n'a donc pas
été flashé ni exécuté sur le matériel. La compilation valide les API et l'édition
de liens ; elle ne valide pas le câblage, le capteur, l'orientation de l'image,
la consommation mémoire pendant l'inférence, la précision ou le débit réel.
Le profil de brochage XIAO Sense n'a pas fait l'objet d'un essai matériel.

Pour la première utilisation : surveiller le démarrage série, tester les cinq
gestes et l'absence de main, puis relever les diagnostics mémoire/temps de cycle
après le chargement des modèles. Tester aussi deux mains et le maintien d'un même
geste pour confirmer le rejet et l'absence de répétition.
