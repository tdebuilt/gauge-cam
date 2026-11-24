# Installation

[English](setup.md) · [Retour au README](../README.fr.md)

## Matériel

| Pièce | Remarque |
|---|---|
| ESP32-CAM AI-Thinker | caméra OV2640, 4 Mo de PSRAM |
| Carte USB ESP32-CAM-MB | pour le flasher et l'alimenter |
| Alimentation 5 V, 1 A | une alimentation faible fait échouer la caméra ou le Wi-Fi |

Fixe la caméra **face** au cadran. Dans une cave, la LED du flash éclaire assez.

## Réseau

L'appareil doit joindre :

- le **serveur MQTT** ;
- un **serveur NTP** (UDP 123), pour dater les lectures et le journal ;
- pour les mises à jour, **au choix** :
  - GitHub en HTTPS : `api.github.com`, `github.com` et `release-assets.githubusercontent.com` ;
  - un miroir local, si l'appareil n'a pas Internet (voir [Mises à jour](updates.fr.md)).

## 1. Flasher

1. Ouvre la [page de flash](https://tdebuilt.github.io/gauge-cam/) dans Chrome ou Edge.
2. Branche l'ESP32-CAM-MB et clique sur *Installer*.
3. Saisis ton Wi-Fi quand la page le demande, puis clique sur *Visit device*.

Si la page n'arrive pas à se connecter, passe la carte en mode téléchargement :

1. Maintiens **IO0** sur la carte MB.
2. Appuie brièvement sur **RST** sur l'ESP32-CAM lui-même. Le bouton RST de la carte MB ne marche
   pas toujours.
3. Relâche **IO0** et clique de nouveau sur *Installer*.

## 2. Les six étapes de réglage

L'appareil ouvre tout seul sa mise en service. Chaque étape peut attendre : clique sur *Plus tard*.
Tant qu'une étape manque, l'onglet **Niveau** la liste avec un bouton *Faire*.

| Étape | Ce que tu fais |
|---|---|
| **Photo** | Régler la lumière et la rotation, pour que le cadran soit droit. |
| **Cadrage** | Tirer les coins autour du cadran. L'appareil trouve le centre de l'aiguille ; un clic le corrige. |
| **Graduations** | Choisir le modèle de jauge, puis cliquer les graduations dans l'ordre (détail ci-dessous). |
| **Vérification** | Faire une lecture d'essai et vérifier que la valeur est juste. |
| **Cuve** | Saisir la forme et la taille de la cuve, ou cliquer sur *Ce n'est pas une cuve* pour un autre cadran. |
| **Home Assistant** | Saisir le nom de l'appareil, le serveur MQTT, l'utilisateur et le mot de passe. |

### Graduations

- **Modèles** : jauge de cuve en cm (un trait tous les 10 cm jusqu'à la fin de l'échelle), 0–100 %,
  ou tout cadran dont tu tapes les valeurs.
- **Clics** : la valeur suivante est proposée toute seule. *Pas de trait à…* saute une valeur que le
  cadran n'imprime pas.
- **Plus de précision** : un clic entre deux graduations ajoute un point ; une bulle demande sa valeur.
- **Corrections** : glisser un point le déplace, cliquer dessus le modifie ou le supprime.
- **Zoom** : molette, deux doigts ou boutons +/−.

### Cuve

Choisis la forme, puis **une** de ces données :

- la capacité inscrite sur la plaque de la cuve et sa hauteur (la fin de l'échelle par défaut) ;
- les dimensions intérieures ;
- une table de jaugeage, du niveau aux litres, pour toute autre forme.

### Home Assistant

*Tester et terminer* n'enregistre la connexion que si elle marche.

Home Assistant tire les identifiants des entités du nom de l'appareil à la première connexion.
Renommer l'appareil ensuite ne les change pas : choisis le nom d'abord.

## 3. Ensuite

Tous les réglages restent dans les onglets **Cadran** et **Réglages**. Chaque changement de prise de
vue reprend une photo sans rien enregistrer :

- intensité de la LED ;
- rotation (0, 90, 180 ou 270°), pour une caméra montée de côté ;
- retournements, résolution, luminosité et contraste ;
- exposition et gain fixes, pour que chaque photo soit identique.

Le cadre, le centre et les graduations suivent la rotation. L'appareil tourne lui-même la photo :
Home Assistant la reçoit aussi à l'endroit.

## Sécurité

- **Mot de passe de l'interface** : facultatif, utilisateur `admin`, dans Réglages → Réseau et MQTT.
- **Les mots de passe ne sont jamais renvoyés** par l'appareil.
- **Le mot de passe MQTT enregistré** ne sert qu'avec le serveur, le port et l'utilisateur pour
  lesquels il a été saisi. En changer un redemande le mot de passe.
- **Point d'accès de secours** : tant que le Wi-Fi n'a jamais marché, l'appareil ouvre aussi un point
  d'accès ouvert, `gauge-cam-xxxxxx`, avec portail captif. Ensuite, il ne s'ouvre que si tu lui as
  donné un mot de passe, après 30 s sans Wi-Fi. Sans ce mot de passe, change le Wi-Fi par USB depuis
  la page de flash.
