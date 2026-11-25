# Fonctionnement

[English](how-it-works.md) · [Retour au README](../README.fr.md)

## Une lecture

1. **Photo.** La LED du flash s'allume et la caméra prend une photo.
2. **Centre.** L'aiguille tourne autour d'un moyeu noir. L'appareil ajuste un cercle sur le contour du
   moyeu, sans le pied de l'aiguille, et le suit d'une photo à l'autre. Le centre peut aussi être
   fixé sur un pixel.
3. **Aiguille.** L'aiguille est le milieu de la bande sombre entre le moyeu et les graduations : un
   reflet le long de l'aiguille ne déplace pas la lecture. Les marques colorées, comme une zone
   rouge, ne sont pas prises pour elle, et leurs valeurs restent lisibles.
4. **Valeur.** La valeur est interpolée entre les deux graduations voisines. Une échelle irrégulière
   passe : un cadran Afriso, par exemple, n'a aucun trait entre 0 et 40 cm.
5. **Volume.** La forme de la cuve convertit le niveau en litres, et le remplissage est le volume
   rapporté à la capacité.

Tout tourne sur l'ESP32, en C++ simple : ni réseau de neurones, ni serveur.

## Contrôles de chaque lecture

| Contrôle | Effet |
|---|---|
| **3 photos** | Chaque lecture planifiée garde la médiane de 3 photos. |
| **Contraste** | Si l'aiguille ressort trop peu (photo sombre), la lecture est refusée et la dernière bonne valeur reste. |
| **Saut** | Un niveau à plus de 5 cm (réglable) du précédent est relu 2 minutes plus tard. Il n'est accepté que si les 2 lectures suivantes le confirment : un remplissage passe, un reflet isolé non. |
| **Lectures refusées** | Les 4 dernières restent dans l'onglet Niveau, avec leur photo. |
| **Lectures manquées** | Après 3 lectures manquées, Home Assistant affiche niveau, volume et remplissage comme indisponibles plutôt qu'une vieille valeur. |

## Sans surveillance

- **Programme bloqué** : un chien de garde le redémarre.
- **Caméra en panne** : l'appareil la relance, puis redémarre, 3 fois au plus depuis la mise sous tension.
- **Réseau perdu** : l'appareil redémarre après 1 h sans Wi-Fi, ou sans le serveur MQTT qu'il avait joint.
- **Plusieurs points d'accès** : l'appareil rejoint le plus fort, et le quitte avant un redémarrage prévu.
  Deux minutes après chaque connexion, il scanne de nouveau et passe, au plus une fois par heure, sur un
  point d'accès plus fort d'au moins 10 dB (après une coupure de courant, le proche peut le refuser un moment).
- **Journal** : Réglages → Système garde les 80 derniers événements et la cause du dernier
  redémarrage (plantage, chien de garde, sous-tension…).
- **Horloge** : un serveur NTP date les lectures, les lectures refusées et le journal. Par défaut
  `pool.ntp.org` ; ta box ou ton routeur conviennent aussi.
