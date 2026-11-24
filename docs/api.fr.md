# API HTTP

[English](api.md) · [Retour au README](../README.fr.md)

L'interface web passe par cette API, et un script peut faire de même. Toutes les réponses sont en
JSON, sauf les photos. Si l'interface a un mot de passe, chaque appel demande une authentification
HTTP basique avec l'utilisateur `admin`.

## Lire

| Appel | Renvoie |
|---|---|
| `GET /api/status` | version, durée de fonctionnement, Wi-Fi, MQTT, état des mises à jour |
| `GET /api/config` | réglages, sans les mots de passe |
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
| `POST /api/config` | enregistre les réglages du corps ; les clés absentes gardent leur valeur |
| `POST /api/mqtt/test` | essaie une connexion MQTT avec les réglages du corps |
| `POST /api/update/check` | cherche une nouvelle version tout de suite |
| `POST /api/update/install` | installe la version trouvée |
| `POST /api/update` | installe le fichier de firmware envoyé |
| `POST /api/restart` | redémarre l'appareil |

## Réglages utiles à connaître

- **Les mots de passe ne sont jamais renvoyés.** `GET /api/config` dit seulement si chacun est
  défini (clés `*_set`).
- **`update_url`** : la source des mises à jour. Vide = GitHub ; sinon l'adresse d'un miroir local.
- **`setup`** : les étapes de mise en service (`photo`, `frame`, `grad`, `check`, `tank`, `ha`),
  chacune `done` ou `later`. `null` remet une étape à faire.

## Exemple : changer un réglage après un essai à blanc

```sh
# quelle lecture avec le centre trouvé sur le moyeu ?
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://APPAREIL/api/test
# le garder
curl -u admin -H 'Content-Type: application/json' -d '{"pivot_mode":"hub"}' http://APPAREIL/api/config
```
