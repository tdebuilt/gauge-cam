# API HTTP

[English](api.md) · [Retour au README](../README.fr.md)

L'interface web passe par cette API, et un script ou un agent IA peut faire de même. Toutes les
réponses sont en JSON, sauf les photos. Si l'interface a un mot de passe, chaque appel demande une
authentification HTTP basique avec l'utilisateur `admin`.

L'appareil décrit lui-même son API : `GET /api` donne les liens, et `GET /api/openapi.json` donne la
description [OpenAPI](../web/openapi.json) complète, avec chaque réglage, son unité et ses limites.
Pour un agent IA, commence par [Piloter une jauge avec un agent IA](agents.fr.md).

## Lire

| Appel | Renvoie |
|---|---|
| `GET /api` | liens vers la description OpenAPI et la documentation |
| `GET /api/openapi.json` | la description OpenAPI 3.1 de cette API |
| `GET /api/status` | version, durée de fonctionnement, Wi-Fi, MQTT, mises à jour, dernières lectures, dernier changement de réglages |
| `GET /api/config` | réglages, sans les mots de passe |
| `GET /api/config/undo` | ce qu'une annulation changerait, sans rien changer |
| `GET /api/photo.jpg` | dernière photo |
| `GET /api/failures` | dernières lectures refusées |
| `GET /api/failure.jpg?id=` | photo d'une lecture refusée |
| `GET /api/log` | 80 derniers événements |
| `GET /api/wifi/scan` | réseaux Wi-Fi à portée |

## Agir

| Appel | Effet |
|---|---|
| `POST /api/capture` | prend une photo ; un corps JSON facultatif essaie des réglages de prise de vue sans les enregistrer |
| `POST /api/read` | lecture complète ; `?photo=0` relit la dernière photo |
| `POST /api/test` | lit la dernière photo avec les réglages du corps, sans les enregistrer |
| `POST /api/config` | enregistre les réglages du corps ; les clés absentes gardent leur valeur ; la réponse dit ce qui a changé |
| `POST /api/config/undo` | remet les réglages d'avant le dernier changement ; une seconde annulation le refait |
| `POST /api/mqtt/test` | essaie une connexion MQTT avec les réglages du corps |
| `POST /api/update/check` | cherche une nouvelle version tout de suite |
| `POST /api/update/install` | installe la version trouvée |
| `POST /api/update` | installe le fichier de firmware envoyé |
| `POST /api/restart` | redémarre l'appareil |

Les erreurs arrivent sous la forme `{"error": "..."}`, avec un code 4xx ou 5xx.

## Réglages utiles à connaître

- **Les mots de passe ne sont jamais renvoyés.** `GET /api/config` dit seulement si chacun est
  défini (clés `*_set`). Un changement apparaît comme `"changed"`, sans la valeur.
- **Ce qui a changé** : `POST /api/config` et `POST /api/config/undo` répondent avec `changes`, par
  exemple `{"pivot_x": {"before": 308.7, "after": 310}}`. Le journal reçoit une ligne
  `Settings changed: pivot_x`, et Home Assistant affiche l'heure dans *Dernier changement de
  réglages*.
- **L'annulation** garde le Wi-Fi, le point d'accès, le mot de passe de l'interface et l'avancement
  de la mise en service actuels : elle ne coupe jamais l'appareil du réseau.
- **`update_url`** : la source des mises à jour. Vide = GitHub ; sinon l'adresse d'un miroir local.
- **`setup`** : les étapes de mise en service (`photo`, `frame`, `grad`, `check`, `tank`, `ha`),
  chacune `done` ou `later`. `null` remet une étape à faire.
- **Les coordonnées** (`roi`, `pivot_x`, `pivot_y`, `points`) sont des pixels de `/api/photo.jpg`,
  déjà tournée selon `rotation`.

## Exemple : changer un réglage après un essai à blanc

```sh
# quelle lecture avec le centre trouvé sur le moyeu ?
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://APPAREIL/api/test
# le garder : la réponse liste ce qui a changé
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://APPAREIL/api/config
# moins bien finalement ? revenir en arrière
curl -u admin -X POST http://APPAREIL/api/config/undo
```
