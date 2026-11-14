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
- **Cuve parallélépipédique** : volume = niveau × longueur × largeur, remplissage = niveau / hauteur.
- **LED du flash** allumée avant chaque photo ; lecture refusée si l'aiguille est trop peu
  contrastée (photo trop sombre), la dernière valeur bonne est gardée.
- **Prise de vue réglée à côté de la photo**, comme dans AI-on-the-edge : intensité de la LED,
  rotation (0, 90, 180, 270°, pour une caméra montée de côté), retournements, résolution, luminosité
  et contraste. Chaque changement reprend une photo sans rien enregistrer ; le contour, le centre et
  les points suivent la rotation. La photo est tournée sur l'ESP32 : Home Assistant la reçoit
  à l'endroit.
- **Home Assistant par découverte MQTT** : niveau, volume, remplissage, photo, bouton « Lire
  maintenant », état de la lecture, angle, contraste et signal Wi-Fi.
- **Interface web** en français et en anglais (autres langues : `web/i18n.js`), mise à jour du
  firmware, sauvegarde et restauration de la configuration.
- **Flash par le navigateur** (Chrome ou Edge) et réglage du Wi-Fi juste après (Improv).
  Sans Wi-Fi, point d'accès `gauge-cam-xxxxxx` et portail captif.

## Matériel

- ESP32-CAM AI-Thinker (OV2640, 4 Mo de PSRAM) et sa carte USB **ESP32-CAM-MB**.
- Alimentation 5 V stable (1 A) : les sous-tensions font échouer la caméra ou le Wi-Fi.
- Caméra fixée **face** au cadran ; la LED suffit dans une cave.

## Installation

1. Ouvre la [page de flash](https://tdelix.github.io/gauge-cam/), branche l'ESP32-CAM-MB et
   clique sur *Installer*.
2. Règle le Wi-Fi quand la page le propose, puis ouvre l'appareil.
3. Onglet **Cadran** : *Nouvelle photo*, règle la lumière et la rotation si besoin, puis sur
   l'image : le contour du cadran, le centre de
   l'aiguille, le début et la fin de l'échelle avec leurs valeurs, puis des points intermédiaires
   partout où l'espacement des graduations change, zone rouge comprise. *Tester*, puis
   *Enregistrer*.
4. Onglet **Cuve** : dimensions intérieures. Onglet **Réseau** : serveur MQTT.

Mise à jour ensuite : onglet Système, fichier `gauge-cam.bin` de la
[dernière version](https://github.com/tdelix/gauge-cam/releases/latest).

## MQTT

| Topic | Contenu |
|---|---|
| `gauge-cam/<id>/state` | JSON : `level`, `volume_l`, `percent`, `angle`, `contrast`, `rssi` (retenu) |
| `gauge-cam/<id>/status` | `ok` ou le message d'erreur de la dernière lecture (retenu) |
| `gauge-cam/<id>/image` | dernière photo JPEG (retenue) |
| `gauge-cam/<id>/availability` | `online` / `offline` |
| `gauge-cam/<id>/cmd` | `read` lance une lecture |

## API HTTP

`GET /api/status`, `GET|POST /api/config`, `POST /api/capture` (corps facultatif : réglages de
prise de vue non enregistrés), `POST /api/read`,
`POST /api/test` (lit la dernière photo avec un réglage non enregistré), `GET /api/photo.jpg`,
`GET /api/wifi/scan`, `POST /api/restart`, `POST /api/update` (firmware). Les mots de passe ne
sont jamais renvoyés. Un mot de passe d'interface (utilisateur `admin`) est possible.

## Développement

```sh
test/host/run.sh                # tests de l'algorithme sur PC (g++, Python + Pillow)
python3 tools/mock_device.py    # interface sur http://127.0.0.1:8080/ sans matériel
pio run && tools/make_factory.sh    # firmware et images dans dist/
```

`lib/gauge` est l'algorithme, en C++ portable ; `src/` le firmware Arduino ; `web/` l'interface,
compressée dans le firmware à la compilation (`tools/prebuild.py`). GitHub Actions teste et
compile chaque push, publie la page de flash depuis `main` et une release pour chaque tag `v*`.

## Licence

MIT.
