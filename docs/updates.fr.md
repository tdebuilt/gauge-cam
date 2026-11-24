# Mises à jour

[English](updates.md) · [Retour au README](../README.fr.md)

## Quelle voie pour ton appareil

| L'appareil… | Fais ceci | Dans Home Assistant |
|---|---|---|
| a Internet | Rien : il interroge GitHub chaque jour. | entité *Firmware* → *Installer* |
| n'a pas Internet | Tiens un **miroir local** et saisis son adresse dans l'appareil. | entité *Firmware* → *Installer* |
| n'a pas Internet, sans miroir | **Envoie** chaque version depuis un ordinateur. | la nouvelle version apparaît une fois installée |

Quelle que soit la voie :

- l'appareil contrôle le fichier avec l'**empreinte SHA-256** publiée avec la version, avant de
  l'installer ;
- si le nouveau firmware plante, ou ne rejoint pas le Wi-Fi en 5 minutes, l'appareil revient au
  précédent ;
- les notes de version s'affichent dans Home Assistant, sur l'entité *Firmware*.

## Avec Internet

Rien à régler. L'appareil interroge GitHub une fois par jour en HTTPS, certificats vérifiés. Le HTTPS
demande l'heure juste : l'appareil doit aussi joindre un serveur NTP.

## Miroir local

Pour un appareil sans Internet. Une machine du réseau qui a Internet (serveur, NAS, Raspberry Pi)
garde une copie de la dernière version.

1. **Lance l'outil chaque jour**, par exemple par cron :

   ```
   15 4 * * *  /opt/gauge-cam/tools/mirror_release.py /srv/www/gauge-cam
   ```

   `tools/mirror_release.py` ne demande que Python. Il télécharge la dernière version, la contrôle
   avec l'empreinte que GitHub publie et garde les 3 dernières versions.
2. **Sers le dossier** avec n'importe quel serveur web statique.
3. **Indique-le à l'appareil** : Réglages → Réseau et MQTT → *Source des mises à jour*, par exemple
   `http://192.168.1.10:8095/gauge-cam`.
4. **Ouvre le pare-feu** de l'appareil vers ce port, et vers rien d'autre.

Le miroir est en HTTP simple, exprès : sans Internet, l'appareil n'a pas forcément l'heure qu'il faut
pour vérifier un certificat. C'est l'empreinte qui protège le fichier, et le miroir ne publie qu'un
fichier qu'il a contrôlé contre GitHub.

## Envoi depuis un ordinateur

```sh
tools/push_firmware.sh http://ADRESSE-DE-L-APPAREIL [vX.Y.Z]
```

Le script télécharge `gauge-cam.bin`, contrôle son empreinte, l'envoie à l'appareil et attend la
nouvelle version. Il demande le mot de passe de l'interface, ou lit `GAUGECAM_PASSWORD`.

Sans miroir, désactive la recherche quotidienne (Réglages → Réseau et MQTT) : elle échouerait chaque
jour.

## À la main

Réglages → Système, puis choisis le fichier `gauge-cam.bin` de la
[dernière version](https://github.com/tdebuilt/gauge-cam/releases/latest).

Cette voie saute le contrôle d'empreinte : le firmware vérifie seulement que l'image est valide.
