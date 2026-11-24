# Home Assistant

[English](home-assistant.md) · [Retour au README](../README.fr.md)

L'appareil s'annonce par la **découverte MQTT**. Si l'intégration MQTT est en place, il n'y a rien
d'autre à faire. Désactiver la découverte dans Réglages → Réseau et MQTT retire les entités.

Les noms des entités suivent la langue de l'appareil (Réglages → Réseau et MQTT) : français ou anglais.

## Entités

| Entité | Contenu |
|---|---|
| Niveau | valeur de l'aiguille, en cm ou dans l'unité du cadran |
| Volume | litres, une fois la cuve réglée |
| Remplissage | % de la capacité |
| Dernière lecture | heure de la dernière lecture **bonne** |
| Photo | dernière photo, à l'endroit |
| Lire maintenant | bouton : lance une lecture tout de suite |
| Firmware | entité de mise à jour : version, notes de version, *Installer* |

Diagnostic : état de la lecture, angle et contraste de l'aiguille, signal Wi-Fi, dernier démarrage et
sa cause, dernier changement de réglages (l'heure du dernier changement du cadran, de la prise de vue
ou de la cuve). La mémoire libre existe aussi, désactivée par défaut.

L'appareil annonce de nouveau ses entités à chaque redémarrage de Home Assistant.

## Topics MQTT

`<id>` est l'identifiant de l'appareil, par exemple `gauge-cam-201baf`. Le préfixe `gauge-cam` se
change.

| Topic | Contenu |
|---|---|
| `gauge-cam/<id>/state` | JSON après chaque bonne lecture : `level`, `volume_l`, `percent`, `angle`, `contrast`, `time` (retenu) |
| `gauge-cam/<id>/diag` | JSON : `rssi`, `boot`, `reset_reason`, `heap_free`, `version`, `config_changed` (retenu) |
| `gauge-cam/<id>/status` | `ok`, ou l'erreur de la dernière lecture (retenu) |
| `gauge-cam/<id>/image` | dernière photo JPEG (retenue) |
| `gauge-cam/<id>/update` | état de l'entité *Firmware* (retenu) |
| `gauge-cam/<id>/availability` | `online` ou `offline` |
| `gauge-cam/<id>/cmd` | envoyer `read` lance une lecture, `install` installe la dernière version |
| `homeassistant/status` | `online` venant de Home Assistant : l'appareil annonce de nouveau ses entités |
