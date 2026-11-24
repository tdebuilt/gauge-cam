# Développement

[English](development.md) · [Retour au README](../README.fr.md)

## Commandes

```sh
test/host/run.sh                    # tests de l'algorithme sur PC (g++, Python + Pillow)
python3 tools/mock_device.py        # l'interface sur http://127.0.0.1:8080/, sans matériel
pio run && tools/make_factory.sh    # firmware et images de flash dans dist/
```

Le simulateur affiche un cadran de cuve dessiné (`tools/oil_dial.py`) et le lit avec le vrai code
C++. Il accepte aussi l'envoi d'un firmware, pour essayer les outils de mise à jour.

## Organisation

| Chemin | Contenu |
|---|---|
| `lib/gauge/` | l'algorithme de lecture, en C++ portable |
| `src/` | le firmware Arduino |
| `web/` | l'interface, compressée dans le firmware à la compilation (`tools/prebuild.py`, 150 Ko au plus) |
| `flasher/index.html` | la page de flash sur GitHub Pages |
| `tools/` | simulateur, images de test, polices, outils de mise à jour |
| `skills/gauge-cam/` | le skill pour les agents IA ([Agents IA](agents.fr.md)) |
| `docs/plan-refonte-ux.md` | choix de conception de l'interface |

L'interface utilise deux polices réduites par `tools/subset_fonts.py` (SIL OFL,
`web/fonts-license.txt`). La page de flash reprend les mêmes. Les autres langues s'ajoutent dans
`web/i18n.js`.

`web/openapi.json` décrit l'API et l'appareil le sert. Le mettre à jour avec chaque route ou
réglage : `test/host/run.sh` échoue s'il lui manque une route de `src/web.cpp`.

## Publier une version

GitHub Actions teste et compile chaque push, et publie la page de flash depuis `main`.

Pour une version :

1. Ajoute une section `## vX.Y.Z` en tête de [`CHANGELOG.md`](../CHANGELOG.md). Elle devient les
   notes de version qu'affiche Home Assistant : les 250 premiers caractères doivent suffire.
2. Pousse un tag `vX.Y.Z`.

Le workflow publie alors la version avec `gauge-cam.bin` et les empreintes SHA-256 dont dépendent
les mises à jour. Un tag sans section dans le changelog n'est pas publié.
