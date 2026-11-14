# Gauge Cam

[English](README.md)

Firmware ESP32-CAM qui lit une jauge à aiguille, par exemple l'indicateur de niveau d'une cuve à
mazout, et publie le niveau, le volume et le remplissage dans Home Assistant par MQTT. Tout se
règle depuis le navigateur, à la manière d'AI-on-the-edge : flash, Wi-Fi, cadran, cuve.

> État : algorithme validé sur la photo d'un vrai cadran Afriso ; tests du dépôt, interface et
> intégration Home Assistant sur PC, avec un cadran de cuve dessiné (`tools/oil_dial.py`) ;
> **pas encore essayé sur un ESP32-CAM réel**.

## Ce qu'il fait

- **Lecture sur l'ESP32**, sans réseau de neurones : le centre de l'aiguille est le moyeu noir
  (suivi d'une photo à l'autre) ou un pixel fixe ; l'aiguille est le rayon le plus sombre entre le
  moyeu et les graduations ; la valeur est interpolée entre les points de calibration voisins, ce
  qui gère les échelles non linéaires (un cadran Afriso n'a aucune graduation entre 0 et 40 cm).
  Les marques colorées (zone rouge) ne sont pas prises pour l'aiguille, et leurs valeurs sont lues.
- **Volume de la cuve** pour un parallélépipède, un cylindre couché ou debout, ou toute autre forme
  par une table de jaugeage niveau → litres ; le remplissage est le volume rapporté à la capacité.
- **Filtre de plausibilité** : chaque lecture prend 3 photos et garde la médiane ; un niveau qui
  s'écarte de plus de 5 cm (réglable) de la lecture précédente est revérifié 2 minutes plus tard
  et accepté seulement si les 2 lectures suivantes le confirment : un remplissage passe, un reflet
  isolé non. Les 4 dernières lectures refusées restent visibles avec leur photo (onglet Niveau).
- **LED du flash** allumée avant chaque photo ; lecture refusée si l'aiguille est trop peu
  contrastée (photo trop sombre), la dernière valeur bonne est gardée. L'exposition et le gain
  peuvent être fixés pour que chaque photo soit identique.
- **Prise de vue réglée à côté de la photo**, comme dans AI-on-the-edge : intensité de la LED,
  rotation (0, 90, 180, 270°, pour une caméra montée de côté), retournements, résolution, luminosité
  et contraste. Chaque changement reprend une photo sans rien enregistrer ; le cadre, le centre et
  les graduations suivent la rotation. La photo est tournée sur l'ESP32 : Home Assistant la reçoit
  à l'endroit.
- **Home Assistant par découverte MQTT** (désactivable, les entités sont alors retirées) : niveau,
  volume, remplissage, heure de la dernière lecture, photo, bouton « Lire maintenant », mise à jour
  du firmware et, en diagnostic, état de la lecture, angle, contraste, signal Wi-Fi, dernier
  démarrage et sa cause, mémoire libre. Annoncées de nouveau à chaque redémarrage de Home
  Assistant. Niveau, volume et remplissage passent à « indisponible » après trois lectures
  manquées, au lieu de garder une vieille valeur.
- **Mise à jour depuis Home Assistant** ou Réglages → Système : l'appareil cherche chaque jour une
  nouvelle version sur GitHub et l'installe en HTTPS (certificats vérifiés). Un nouveau firmware qui
  plante, ou qui ne rejoint pas le Wi-Fi en 5 minutes, est remplacé par le précédent au redémarrage.
- **Fonctionne sans surveillance** : un chien de garde redémarre un programme bloqué ; la caméra est
  relancée, puis l'appareil redémarre (3 fois au plus depuis la mise sous tension) quand elle
  échoue ; l'appareil redémarre aussi après 1 h sans Wi-Fi, ou sans le serveur MQTT qu'il avait
  joint. Réglages → Système garde un journal des 80 derniers événements et dit pourquoi l'appareil a
  redémarré (plantage, chien de garde, sous-tension…).
- **Horloge** par un serveur NTP (`pool.ntp.org` par défaut, ou la passerelle) : les lectures, les
  lectures refusées et le journal sont datés.
- **Interface web** en français et en anglais (autres langues : `web/i18n.js`), mise à jour du
  firmware, sauvegarde et restauration de la configuration. Les mots de passe ne sont jamais
  renvoyés, et le mot de passe MQTT enregistré ne sert qu'avec le serveur pour lequel il a été
  saisi : changer de serveur, de port ou d'utilisateur demande de le ressaisir.
- **Flash par le navigateur** (Chrome ou Edge) et réglage du Wi-Fi juste après, par USB (Improv).
  Tant que le Wi-Fi n'a jamais fonctionné, l'appareil ouvre aussi un point d'accès ouvert
  `gauge-cam-xxxxxx` avec portail captif. Une fois le Wi-Fi validé, ce point d'accès de secours ne
  s'ouvre plus (après 30 s sans Wi-Fi) que s'il a reçu un mot de passe ; sinon le Wi-Fi se change
  par USB depuis la page de flash.

## Matériel

- ESP32-CAM AI-Thinker (OV2640, 4 Mo de PSRAM) et sa carte USB **ESP32-CAM-MB**.
- Alimentation 5 V stable (1 A) : les sous-tensions font échouer la caméra ou le Wi-Fi.
- Caméra fixée **face** au cadran ; la LED suffit dans une cave.

Réseau : le serveur MQTT ; pour les mises à jour, HTTPS vers `api.github.com`, `github.com` et
`release-assets.githubusercontent.com` ; NTP (UDP 123) vers le serveur de temps choisi.

## Installation

1. Ouvre la [page de flash](https://tdebuilt.github.io/gauge-cam/), branche l'ESP32-CAM-MB et
   clique sur *Installer*.
2. Règle le Wi-Fi quand la page le propose, puis *Visit device* : l'appareil ouvre sa **mise en
   service**, six étapes qui peuvent chacune attendre (*Plus tard*) :
   - *Photo* (lumière et rotation) ;
   - *Cadrage* (tirer les coins autour du cadran ; le centre de l'aiguille est trouvé tout seul, un
     clic le corrige) ;
   - *Graduations* (choisir le modèle de jauge : jauge de cuve en cm, un trait tous les 10 cm
     jusqu'à la fin de son échelle, 0–100 %, ou tout autre cadran dont on tape les valeurs ; puis
     cliquer les graduations dans l'ordre : la valeur suivante est proposée toute seule, *Pas de
     trait à…* saute celle que le cadran n'a pas ; un clic entre deux graduations ajoute un point et
     une bulle demande sa valeur ; un clic sur un point le corrige ou le supprime ; zoom à la
     molette, à deux doigts ou par +/− pour placer au plus juste) ;
   - *Vérification* (lecture d'essai : la valeur est-elle juste ?) ;
   - *Cuve* (forme, puis capacité inscrite sur la plaque et hauteur, proposée à la fin de
     l'échelle ; ou les dimensions intérieures, ou une table de jaugeage ; *Ce n'est pas une cuve*
     pour tout autre cadran) ;
   - *Home Assistant* (nom de l'appareil, serveur MQTT, utilisateur et mot de passe ; *Tester et
     terminer* n'enregistre qu'une connexion qui marche). Home Assistant tire du nom les
     identifiants des entités à la première connexion : renommer l'appareil ensuite ne les change
     pas.
3. Tant qu'une étape manque, l'onglet Niveau la liste avec un bouton *Faire*. Les mêmes réglages
   restent dans les onglets **Cadran** et **Réglages**.

Mise à jour ensuite : entité *Firmware* dans Home Assistant, Réglages → Système (*Rechercher une
mise à jour*), ou à la main avec le fichier `gauge-cam.bin` de la
[dernière version](https://github.com/tdebuilt/gauge-cam/releases/latest).

## MQTT

| Topic | Contenu |
|---|---|
| `gauge-cam/<id>/state` | JSON : `level`, `volume_l`, `percent`, `angle`, `contrast`, `time`, envoyé après chaque lecture réussie (retenu) |
| `gauge-cam/<id>/diag` | JSON : `rssi`, `boot`, `reset_reason`, `heap_free`, `version` (retenu) |
| `gauge-cam/<id>/update` | état de l'entité de mise à jour du firmware de Home Assistant (retenu) |
| `gauge-cam/<id>/status` | `ok` ou le message d'erreur de la dernière lecture (retenu) |
| `gauge-cam/<id>/image` | dernière photo JPEG (retenue) |
| `gauge-cam/<id>/availability` | `online` / `offline` |
| `gauge-cam/<id>/cmd` | `read` lance une lecture, `install` installe la dernière version |
| `homeassistant/status` | `online` (Home Assistant a redémarré) : les entités sont annoncées de nouveau |

## API HTTP

`GET /api/status`, `GET|POST /api/config`, `POST /api/capture` (corps facultatif : réglages de
prise de vue non enregistrés), `POST /api/read`,
`POST /api/test` (lit la dernière photo avec un réglage non enregistré), `GET /api/photo.jpg`,
`GET /api/failures` et `GET /api/failure.jpg?id=` (lectures refusées), `GET /api/log`,
`POST /api/update/check` et `POST /api/update/install` (version publiée sur GitHub),
`POST /api/mqtt/test` (une connexion avec des réglages MQTT non enregistrés), `GET /api/wifi/scan`,
`POST /api/restart`, `POST /api/update` (firmware). La clé `setup` de la configuration porte les
étapes de la mise en service (`photo`, `frame`, `grad`, `check`, `tank`, `ha`) marquées `done` ou
`later` ; `null` remet une étape à faire. Les mots de passe ne sont jamais renvoyés. Un mot de passe d'interface (utilisateur `admin`) est possible.

## Développement

```sh
test/host/run.sh                # tests de l'algorithme sur PC (g++, Python + Pillow)
python3 tools/mock_device.py    # interface sur http://127.0.0.1:8080/ sans matériel
pio run && tools/make_factory.sh    # firmware et images dans dist/
```

`lib/gauge` est l'algorithme, en C++ portable ; `src/` le firmware Arduino ; `web/` l'interface,
compressée dans le firmware à la compilation (`tools/prebuild.py`, 150 Ko au plus), avec deux polices
réduites par `tools/subset_fonts.py` (SIL OFL, `web/fonts-license.txt`) ; `flasher/index.html` la
page de flash, qui reprend les mêmes polices. Choix de l'interface : `docs/plan-refonte-ux.md`.
GitHub Actions teste et compile chaque push, publie la page de flash depuis `main` et une release
pour chaque tag `v*`.

## Licence

MIT.
