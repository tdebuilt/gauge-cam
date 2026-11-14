# Plan de la refonte de l'interface

Décisions validées le 2026-11-14 (`maquette/propositions/validation-b.md`). Référence visuelle :
`maquette/propositions/b.html` (direction B, Instrument) et `a-plus.html` (parcours de mise en service).
Maquettes reconstruites par `build.py` et `build_aplus.py` ; elles ne touchent ni le firmware ni l'appareil.

## Décisions retenues

| Sujet | Décision |
|---|---|
| Identité | Direction B : instrument sombre, accent ambre `#f5a524`, chiffres en IBM Plex Mono, titres en Manrope, texte en Inter. Thème clair par `prefers-color-scheme`, sans bouton |
| Navigation | Trois onglets : **Niveau** (accueil), **Cadran**, **Réglages**. Les six onglets actuels s'y rangent : `reading` → Niveau ; `dial` → Cadran ; `tank`, `camera`, `network`, `system` → sections de Réglages |
| Accueil | Jauge en arc de 240° avec zone rouge, chiffre principal en litres (en cm tant que la cuve n'est pas décrite), trio remplissage / niveau ou volume / prochaine lecture, dernière photo avec repères |
| Points d'échelle | Bulle de saisie posée sur la photo (repris de D) pour un point hors modèle |
| Ce qui manque | Liste sur l'accueil tant que la mise en service n'est pas finie, un bouton « Faire » par étape (repris de C) |
| Écartés | Historique local de D ; assistant complet de C |
| Mise en service | « Visit device » de la page d'installation, puis bienvenue et six étapes, chacune remise à plus tard : Photo, Cadrage, Graduations, Vérification, Cuve, Home Assistant |
| Photo | Flash, rotation, miroir ; le reste sous « Plus de réglages » |
| Cadrage | Un cadre à quatre coins (le `roi` actuel) ; centre trouvé par le mode `hub`, corrigeable d'un clic |
| Graduations | Modèle de jauge (mazout 0–150 cm : 0, 40, 50 … 150 ; 0–100 % ; saisie libre), clics dans l'ordre, valeur suivante proposée, « Annuler le dernier » |
| Vérification | Lecture d'essai (`/api/test`) : « L'appareil lit X. Est-ce ce que montre la jauge ? » ; « Non » ramène aux graduations |
| Cuve | Forme et capacité de la plaque ; hauteur proposée à la fin de l'échelle ; dimensions calculées par l'interface. Dimensions et table de jaugeage restent possibles |
| Home Assistant | Serveur, utilisateur, mot de passe ; « Tester et terminer » ne valide qu'une connexion réussie |
| Page d'installation | Même identité que l'appareil, FR/EN, bouton esp-web-tools, version affichée |

## Contraintes

- Flash : environ 600 Ko libres (partitions `min_spiffs`, firmware de 1 305 568 octets au 2026-11-14).
  Budget de l'interface embarquée : **150 Ko gzippés** en tout, polices comprises ; `tools/prebuild.py`
  échoue au-delà.
- Polices embarquées en sous-ensemble woff2 Latin-1 (`tools/subset_fonts.py`, 11 Ko chacune) : IBM Plex
  Mono renommée Gauge Mono (« Plex » est un nom réservé de la licence SIL OFL) pour les chiffres, Manrope
  pour la marque. Le texte courant prend la police du système plutôt qu'Inter : trois fichiers de plus
  pour un gain faible. Notice : `web/fonts-license.txt`.
- Aucun appel externe depuis l'interface de l'appareil : elle doit marcher sans Internet (point d'accès).
- Les réglages envoyés restent ceux d'aujourd'hui (`/api/config`) : une ancienne configuration se relit
  sans migration.

## Étapes

Chacune est publiable seule : tag `vX.Y.0`, release par la CI, mise à jour depuis Home Assistant. Une
étape n'est taguée qu'après essai sur l'ESP32-CAM.

### 1. v0.2.0 — Socle visuel et trois onglets

- Contenu : jetons de couleur et de typographie de B, thème clair, polices en sous-ensemble ; en-tête
  avec voyants Wi-Fi / MQTT / Caméra et choix FR/EN ; trois onglets ; accueil avec la jauge, le chiffre
  et le trio ; Réglages en sections repliables (Cuve, Caméra, Réseau, Système).
- Fichiers : `web/index.html`, `web/style.css`, `web/app.js`, `web/i18n.js`, `web/fonts/` ;
  `tools/prebuild.py` (budget de taille).
- Tests : `test/host/run.sh` inchangé ; `tools/mock_device.py` puis captures Playwright des trois
  onglets en sombre et en clair, à 1200 px et à 390 px, zéro erreur console ; taille gzippée affichée
  par le build.

### 2. v0.3.0 — Onglet Cadran

- Contenu : cadre à quatre coins sur la photo (écrit `roi`), marque du centre (`pivot_mode`,
  `pivot_x`, `pivot_y`), graduations par modèle et clics dans l'ordre, bulle pour un point en plus,
  « Annuler le dernier », lecture d'essai.
- Fichiers : `web/app.js`, `web/index.html`, `web/style.css`, `web/i18n.js`. Firmware inchangé.
- Tests : parcours Playwright sur le simulateur (13 clics, aucune saisie, points envoyés identiques à
  ceux d'aujourd'hui) ; essai sur l'appareil face à la jauge.
- Fait le 2026-11-14, essai sur l'appareil à faire. Quatre étapes dans l'onglet (Photo, Cadrage,
  Graduations, Vérification), les noms de la future mise en service. Écarts au plan :
  - centre trouvé par l'interface (tache sombre la plus contrastée au milieu du cadre), puisque le
    firmware ne cherche le moyeu qu'avec une échelle ; essai : 2 px de la vérité sur le simulateur,
    7 px sur la photo de la jauge, le mode `hub` affine ensuite ;
  - modèle 0–100 % : 0, 25, 50, 75, 100 ;
  - Graduations et Vérification montrent l'intérieur du cadre agrandi à la largeur de la colonne ;
    zoom jusqu'à ×8 en plus (molette, deux doigts, boutons +/−), photo déplacée en la glissant,
    repères redessinés à la taille de l'écran pour rester nets, points gardés au dixième de pixel ;
  - « Enregistrer » seulement à l'étape Vérification, après la lecture d'essai ;
  - sans cadre enregistré, l'appareil lit toute la photo comme avant : `roi` reste `[0, 0, 0, 0]`
    tant que le cadre n'est pas touché.

### 3. v0.4.0 — Mise en service

- Contenu : écran de bienvenue et six étapes, « Plus tard », liste « ce qui manque » sur l'accueil,
  étape Vérification, Cuve par la capacité (calcul côté interface : parallélépipède, cylindre couché,
  cylindre debout), Home Assistant avec test.
- Firmware :
  - champ `setup` dans la configuration (étapes faites ou remises à plus tard), lu par l'accueil ;
  - `POST /api/mqtt/test` : connexion d'essai avec les valeurs saisies, réponse `ok` ou l'erreur du
    broker, sans rien enregistrer ;
  - `/` ouvre la mise en service tant que `setup` n'est pas fini (lien « Visit device » d'Improv).
- Fichiers : `src/config.h`, `src/config.cpp`, `src/web.cpp`, `src/mqtt.cpp` ; `web/*` ;
  `tools/mock_device.py` (les deux nouveautés) ; `test/host` (calcul de capacité si porté en C++).
- Tests : parcours complet en 25 gestes sur le simulateur ; « Plus tard » à chaque étape puis reprise
  depuis l'accueil ; test MQTT réussi et refusé contre le CT 110 (mosquitto) ; mise en service réelle.
- Fait le 2026-11-14, essai sur l'appareil à faire. Écarts au plan :
  - `/` sert toujours la même page : c'est l'interface qui ouvre la mise en service en lisant
    `setup`, tant qu'une étape n'est ni faite ni remise à plus tard ;
  - `setup` vaut `{"photo": "done", "tank": "later", …}` ; une étape absente reste à faire. Cuve et
    Home Assistant comptent comme faits dès qu'ils sont réglés dans Réglages, le cadran dès qu'il est
    enregistré à l'étape Vérification. Un appareil déjà en service (configuration sans `setup`,
    cadran réglé) passe à la mise à jour en « fait » pour ce qui marche, « plus tard » pour le reste ;
  - « Plus tard » sur une étape du cadran remet les quatre ; chaque étape quittée enregistre ce qui
    est déjà fait (cadre, centre, puis graduations quand elles forment une échelle) ;
  - Cuve par la capacité aussi dans Réglages : l'interface en tire des dimensions qui donnent les
    mêmes litres à tout niveau (côté carré, diamètre, ou longueur du cylindre couché) ;
  - bouton « Tester la connexion » dans Réglages → Réseau et MQTT ;
  - parcours mesuré : 26 gestes, contre 25 comptés sur la maquette (cadran, capacité, trois champs MQTT) ;
  - test MQTT : refus vérifié contre le CT 110 (mosquitto) par le simulateur, qui fait la même
    connexion en Python ; réussite vérifiée contre un faux serveur local ; la réussite réelle se
    fera sur l'appareil avec son compte MQTT.

### 4. v0.5.0 — Page d'installation

- Contenu : `flasher/index.html` dans l'identité B (hero avec la jauge, bouton d'installation, version,
  étapes FR/EN, aide au bouton IO0), polices servies par Pages.
- Fichiers : `flasher/index.html`, `.github/workflows/build.yml` (copie des polices dans le site).
- Tests : chargement Chromium de `tdebuilt.github.io/gauge-cam/`, élément `esp-web-install-button`
  défini, zéro erreur ; flash réel par USB puis « Visit device ».

Cette étape ne touche pas le firmware : elle peut passer avant les autres.

## Hors champ

- Historique local des lectures (Home Assistant le garde).
- Bouton pour forcer le thème (à ajouter dans Réglages sur demande).
