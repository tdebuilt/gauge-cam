# Piloter une jauge avec un agent IA

[English](agents.md) · [Retour au README](../README.fr.md)

Un agent IA qui peut lancer des commandes (Claude Code, Codex et autres) sait lire la jauge,
expliquer une lecture fausse et régler le cadran à ta place. Il passe par la même [API HTTP](api.fr.md)
que l'interface.

## Donner à l'agent ce qu'il lui faut

1. **Le réseau** : l'agent tourne sur un ordinateur qui atteint l'appareil, par exemple
   `http://192.168.1.50`.
2. **L'adresse et le mot de passe** dans deux variables d'environnement, jamais dans la
   conversation :

   ```sh
   export GAUGE_CAM_URL=http://192.168.1.50
   export GAUGE_CAM_PASSWORD='le mot de passe de l interface'   # vide s'il n'y en a pas
   ```

3. **Le skill** : copie [`skills/gauge-cam/`](../skills/gauge-cam/SKILL.md) dans le dossier des
   skills de ton agent, par exemple `~/.claude/skills/gauge-cam/` pour Claude Code ou
   `~/.codex/skills/gauge-cam/` pour Codex. Il explique à l'agent comment appeler l'API sans montrer
   le mot de passe, et les règles ci-dessous.

Sans le skill, indique à l'agent `GET /api/openapi.json` : l'appareil y décrit lui-même chaque route
et chaque réglage, avec les mêmes règles en introduction.

## Les règles que suit l'agent

| Étape | Appel | Pourquoi |
|---|---|---|
| Regarder | `GET /api/status`, `/api/config`, `/api/photo.jpg`, `/api/log` | Comprendre avant de changer quoi que ce soit |
| Essayer | `POST /api/test` avec des réglages en brouillon | Relit la dernière photo, n'enregistre rien |
| Demander | avant et après montrés à toi | Rien n'est enregistré sans ton accord |
| Enregistrer | `POST /api/config` avec les seules clés changées | La réponse liste ce qui a changé |
| Vérifier | `POST /api/read` | Nouvelles photos, vraie lecture |
| Annuler | `POST /api/config/undo` | Retour aux réglages d'avant le changement |

L'agent ne touche jamais au Wi-Fi, aux mots de passe ni à MQTT sans ta demande, et demande avant une
mise à jour ou un redémarrage.

## Ce que tu peux lui demander

- « Quel est le niveau de mazout, et à quand remonte la dernière bonne lecture ? »
- « Le niveau a sauté cet après-midi : pourquoi ? » L'agent lit le journal (lignes
  `Settings changed`, redémarrages, lectures refusées) et compare les photos.
- « Vérifie le centre de l'aiguille et les graduations sur la photo du jour. » L'agent les place sur
  la photo, les essaie et te montre la différence avant d'enregistrer.

## Sécurité

- L'annulation vaut pour le dernier changement ; une seconde annulation le refait. Garde aussi une
  copie des réglages (*Réglages → Système → Exporter*) : un reflash les efface.
- Le mot de passe de l'interface protège toute l'API. Si l'agent tourne sans surveillance, donne-lui
  son propre compte sur l'ordinateur, pas le tien.
