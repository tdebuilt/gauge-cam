"use strict";
// Interface translations. Add a language by adding a dictionary with the
// same keys; missing keys fall back to English.
//
// In the HTML: data-i18n="key" sets the text, data-i18n-html="key" the
// markup (trusted strings below only), data-i18n-placeholder / -title the
// attributes. In the code: t("key", {name: value}) fills {name}.

const I18N = {
  fr: {
    "lang.name": "Français",
    "tab.reading": "Lecture", "tab.dial": "Cadran", "tab.tank": "Cuve",
    "tab.camera": "Mesure", "tab.network": "Réseau", "tab.system": "Système",
    "common.save": "Enregistrer",

    "r.level": "Niveau", "r.volume": "Volume", "r.percent": "Remplissage",
    "r.read": "Lire maintenant",
    "r.notReady": "Cadran pas encore réglé : onglet Cadran.",
    "r.ok": "Lecture réussie (contraste de l'aiguille {contrast}).",
    "r.last": "dernière lecture il y a {age}", "r.never": "aucune lecture réussie",
    "r.next": "prochaine dans {in}",
    "r.failures": "Dernières lectures refusées",
    "r.failures.help": "Les quatre dernières, avec leur photo, depuis le dernier démarrage.",
    "r.failureAge": "il y a {age}", "r.failureRead": "lu : {value} {unit}", "r.noPhoto": "pas de photo",

    "conn.ap": "point d'accès", "conn.ok": "MQTT connecté", "conn.down": "MQTT déconnecté",
    "conn.none": "MQTT non réglé", "conn.offline": "hors ligne",

    "d.help": "Prends une photo, puis place chaque élément en cliquant sur l'image : le <b>contour</b> du cadran (glisser), le <b>centre</b> de l'aiguille, le <b>début</b> et la <b>fin</b> de l'échelle, puis des <b>points</b> intermédiaires. Ajoute des points partout où l'espacement des graduations change, <b>zone rouge comprise</b> : la lecture est interpolée entre deux points voisins.",
    "d.photo": "Nouvelle photo",
    "d.shot": "Prise de vue",
    "d.shot.help": "Chaque changement reprend une photo pour juger de l'effet. Rien n'est gardé avant « Enregistrer ». Le contour, le centre et les points suivent la rotation.",
    "d.rotation": "Rotation (sens horaire)",
    "d.shotFailed": "Photo impossible : {error}",
    "d.tool.roi": "Contour", "d.tool.pivot": "Centre", "d.tool.start": "Début",
    "d.tool.end": "Fin", "d.tool.point": "Point",
    "d.zoom": "Zoom", "d.zoom.fit": "ajusté",
    "d.mode": "Centre de l'aiguille", "d.mode.hub": "suit le moyeu noir (conseillé)",
    "d.mode.fixed": "fixe, au pixel cliqué",
    "d.unit": "Unité du cadran", "d.scale": "Échelle",
    "d.col.value": "Valeur", "d.col.position": "Position",
    "d.test": "Tester",
    "d.row.start": "Début", "d.row.end": "Fin", "d.row.point": "Point {n}",
    "d.toPlace": "à placer", "d.move": "Déplacer", "d.off": "éteinte", "d.delete": "Supprimer",
    "d.hint.roi": "Glisse sur l'image pour entourer le cadran. La lecture ignore tout ce qui est dehors.",
    "d.hint.pivot": "Clique au centre du moyeu noir, là où tourne l'aiguille.",
    "d.hint.start": "Clique sur la graduation du début de l'échelle (par exemple le 0, au bout de la zone rouge).",
    "d.hint.end": "Clique sur la graduation de la fin de l'échelle (par exemple 150).",
    "d.hint.point": "Clique sur une graduation, puis saisis sa valeur dans le tableau. Ajoutes-en dans la zone rouge.",
    "d.hint.move": "Clique sur la nouvelle position du point.",
    "d.err.pivot": "Place le centre de l'aiguille.",
    "d.err.ends": "Place le début et la fin de l'échelle.",
    "d.err.values": "Chaque point doit avoir une valeur.",
    "d.err.same": "Le début et la fin doivent avoir des valeurs différentes.",
    "d.err.range": "Les points intermédiaires doivent être entre {lo} et {hi}.",
    "d.result": "Lecture : {value} {unit} (aiguille à {angle}°, contraste {contrast}).",
    "d.saved": "Réglage du cadran enregistré.",

    "t.help": "Cuve parallélépipédique : dimensions <b>intérieures</b>. Le volume vaut niveau × longueur × largeur.",
    "t.length": "Longueur (cm)", "t.width": "Largeur (cm)", "t.height": "Hauteur (cm)",
    "t.offset": "Décalage du niveau (cm)", "t.offset.help": "hauteur réelle quand le cadran indique 0",
    "t.capacity": "Capacité : {cap} L · {perCm} L par cm de hauteur.",
    "t.perCm": "{perCm} L par cm de hauteur.",

    "c.interval": "Lecture toutes les (minutes)", "c.framesize": "Résolution",
    "c.vflip": "Retourner verticalement", "c.hmirror": "Miroir horizontal",
    "c.led": "Intensité de la LED", "c.ledMs": "Allumage avant la photo (ms)",
    "c.brightness": "Luminosité", "c.contrast": "Contraste",
    "c.jpeg": "Qualité JPEG (4 = meilleure)",
    "c.minContrast": "Contraste minimal de l'aiguille",
    "c.minContrast.help": "en dessous, la lecture est refusée (photo trop sombre)",
    "c.samples": "Photos par lecture",
    "c.samples.help": "la valeur médiane est gardée : un reflet sur une photo ne fausse pas la lecture",
    "c.maxJump": "Écart maximal sans confirmation",
    "c.maxJump.help": "dans l'unité du cadran ; un écart plus grand avec la dernière lecture est revérifié 2 min plus tard et accepté seulement si les 2 lectures suivantes le confirment (remplissage). 0 = désactivé",
    "c.moved": "La lumière, la rotation et la résolution de la caméra se règlent dans l'onglet Cadran, à côté de la photo.",

    "n.name": "Nom de l'appareil", "n.lang": "Langue des entités Home Assistant",
    "n.wifi": "Wi-Fi", "n.ssid": "Réseau", "n.scan": "Chercher les réseaux",
    "n.password": "Mot de passe", "n.mqtt": "MQTT", "n.host": "Serveur", "n.port": "Port",
    "n.user": "Utilisateur", "n.prefix": "Préfixe des topics", "n.discovery": "Préfixe de découverte",
    "n.access": "Accès à cette interface", "n.uiPassword": "Mot de passe (utilisateur admin)",
    "n.uiClear": "Supprimer le mot de passe",
    "n.discoveryOn": "Découverte automatique dans Home Assistant",
    "n.apPassword": "Mot de passe du point d'accès de secours",
    "n.apPassword.help": "8 caractères au moins. Le point d'accès s'ouvre après 30 s sans Wi-Fi. Sans mot de passe, il ne s'ouvre plus une fois le Wi-Fi validé : on change alors le Wi-Fi par USB, depuis la page de flash.",
    "n.apClear": "Supprimer le mot de passe du point d'accès",
    "n.unchanged": "inchangé", "n.nonePw": "aucun",
    "n.found": "{n} réseaux trouvés.", "n.open": "ouvert",

    "s.update": "Mise à jour",
    "s.update.help": "Fichier <code>gauge-cam.bin</code> d'une version publiée, ou réinstallation complète par <a href=\"https://tdelix.github.io/gauge-cam/\" target=\"_blank\" rel=\"noopener\">la page de flash</a>.",
    "s.updateButton": "Mettre à jour", "s.backup": "Sauvegarde",
    "s.backup.help": "Les mots de passe ne sont pas exportés.",
    "s.export": "Exporter la configuration", "s.import": "Importer", "s.restart": "Redémarrer",
    "s.version": "Version", "s.id": "Identifiant", "s.uptime": "En marche depuis",
    "s.wifi": "Wi-Fi", "s.ap": "Point d'accès", "s.mqtt": "MQTT", "s.photo": "Photo",
    "s.memory": "Mémoire libre", "s.disconnected": "déconnecté", "s.closed": "fermé",
    "s.connected": "connecté", "s.notSet": "non réglé", "s.none": "aucune",
    "s.apOn": "{name} sur {ip}", "s.kb": "Ko",
    "s.chooseFile": "Choisis d'abord le fichier .bin.",
    "s.updated": "Mise à jour installée, redémarrage…",
    "s.updateFailed": "Échec de la mise à jour : {error}", "s.sendFailed": "Échec de l'envoi.",
    "s.imported": "Configuration importée.", "s.importedRestart": "Configuration importée, redémarrage…",
    "s.restarting": "Redémarrage…",

    "saved": "Enregistré.", "savedRestart": "Enregistré. Redémarrage sur le nouveau réseau Wi-Fi…",
    "unit.s": "{n} s", "unit.min": "{n} min", "unit.h": "{n} h", "unit.d": "{n} j",

    "err.hub": "Moyeu de l'aiguille introuvable près du centre : replace le centre ou passe en mode fixe.",
    "err.contrast": "Aiguille trop peu contrastée : vérifie l'éclairage (LED) ou baisse le contraste minimal.",
    "err.order": "Les points ne sont pas dans l'ordre du cadran autour du centre : vérifie le centre et les valeurs.",
    "err.notConfigured": "Cadran à régler : centre, début et fin de l'échelle.",
    "err.band": "Graduations trop proches du centre : le centre est sans doute mal placé.",
    "err.noPhoto": "Pas encore de photo.",
    "err.camera": "Erreur de caméra : {error}",
    "err.jump": "Saut improbable de {from} à {to} : nouvelle vérification dans 2 min ({n}/3).",
  },

  en: {
    "lang.name": "English",
    "tab.reading": "Reading", "tab.dial": "Dial", "tab.tank": "Tank",
    "tab.camera": "Measurement", "tab.network": "Network", "tab.system": "System",
    "common.save": "Save",

    "r.level": "Level", "r.volume": "Volume", "r.percent": "Fill level",
    "r.read": "Read now",
    "r.notReady": "Dial not set up yet: see the Dial tab.",
    "r.ok": "Reading succeeded (needle contrast {contrast}).",
    "r.last": "last reading {age} ago", "r.never": "no successful reading",
    "r.next": "next in {in}",
    "r.failures": "Last refused readings",
    "r.failures.help": "The last four, with their photo, since the last start.",
    "r.failureAge": "{age} ago", "r.failureRead": "read: {value} {unit}", "r.noPhoto": "no photo",

    "conn.ap": "access point", "conn.ok": "MQTT connected", "conn.down": "MQTT disconnected",
    "conn.none": "MQTT not set", "conn.offline": "offline",

    "d.help": "Take a photo, then place each element by clicking the image: the dial <b>outline</b> (drag), the needle <b>centre</b>, the <b>start</b> and <b>end</b> of the scale, then intermediate <b>points</b>. Add points wherever the spacing of the graduations changes, <b>red zone included</b>: the reading is interpolated between two neighbouring points.",
    "d.photo": "New photo",
    "d.shot": "Capture",
    "d.shot.help": "Each change takes a new photo so you can judge the effect. Nothing is kept until Save. The outline, centre and points follow the rotation.",
    "d.rotation": "Rotation (clockwise)",
    "d.shotFailed": "Cannot take the photo: {error}",
    "d.tool.roi": "Outline", "d.tool.pivot": "Centre", "d.tool.start": "Start",
    "d.tool.end": "End", "d.tool.point": "Point",
    "d.zoom": "Zoom", "d.zoom.fit": "fit",
    "d.mode": "Needle centre", "d.mode.hub": "follows the dark hub (advised)",
    "d.mode.fixed": "fixed, at the clicked pixel",
    "d.unit": "Dial unit", "d.scale": "Scale",
    "d.col.value": "Value", "d.col.position": "Position",
    "d.test": "Test",
    "d.row.start": "Start", "d.row.end": "End", "d.row.point": "Point {n}",
    "d.toPlace": "to place", "d.move": "Move", "d.off": "off", "d.delete": "Delete",
    "d.hint.roi": "Drag on the image around the dial. Anything outside is ignored.",
    "d.hint.pivot": "Click the centre of the dark hub the needle turns around.",
    "d.hint.start": "Click the graduation where the scale starts (for example 0, at the end of the red zone).",
    "d.hint.end": "Click the graduation where the scale ends (for example 150).",
    "d.hint.point": "Click a graduation, then type its value in the table. Add some in the red zone.",
    "d.hint.move": "Click the new position of the point.",
    "d.err.pivot": "Place the needle centre.",
    "d.err.ends": "Place the start and the end of the scale.",
    "d.err.values": "Every point needs a value.",
    "d.err.same": "Start and end need different values.",
    "d.err.range": "Intermediate points must be between {lo} and {hi}.",
    "d.result": "Reading: {value} {unit} (needle at {angle}°, contrast {contrast}).",
    "d.saved": "Dial settings saved.",

    "t.help": "Rectangular tank: <b>inner</b> dimensions. Volume is level × length × width.",
    "t.length": "Length (cm)", "t.width": "Width (cm)", "t.height": "Height (cm)",
    "t.offset": "Level offset (cm)", "t.offset.help": "actual level when the dial shows 0",
    "t.capacity": "Capacity: {cap} L · {perCm} L per cm of height.",
    "t.perCm": "{perCm} L per cm of height.",

    "c.interval": "Read every (minutes)", "c.framesize": "Resolution",
    "c.vflip": "Flip vertically", "c.hmirror": "Mirror horizontally",
    "c.led": "LED intensity", "c.ledMs": "LED on before the photo (ms)",
    "c.brightness": "Brightness", "c.contrast": "Contrast",
    "c.jpeg": "JPEG quality (4 = best)",
    "c.minContrast": "Minimum needle contrast",
    "c.minContrast.help": "below it the reading is rejected (photo too dark)",
    "c.samples": "Photos per reading",
    "c.samples.help": "the median value is kept: a reflection on one photo does not spoil the reading",
    "c.maxJump": "Largest change without confirmation",
    "c.maxJump.help": "in the dial unit; a larger change from the last reading is checked again 2 min later and accepted only if the next 2 readings confirm it (refill). 0 = off",
    "c.moved": "The camera light, rotation and resolution are set in the Dial tab, next to the photo.",

    "n.name": "Device name", "n.lang": "Home Assistant entity language",
    "n.wifi": "Wi-Fi", "n.ssid": "Network", "n.scan": "Scan networks",
    "n.password": "Password", "n.mqtt": "MQTT", "n.host": "Server", "n.port": "Port",
    "n.user": "User", "n.prefix": "Topic prefix", "n.discovery": "Discovery prefix",
    "n.access": "Access to this interface", "n.uiPassword": "Password (user admin)",
    "n.uiClear": "Remove the password",
    "n.discoveryOn": "Home Assistant automatic discovery",
    "n.apPassword": "Fallback access point password",
    "n.apPassword.help": "At least 8 characters. The access point opens after 30 s without Wi-Fi. Without a password it no longer opens once Wi-Fi works: change Wi-Fi over USB from the flasher page instead.",
    "n.apClear": "Remove the access point password",
    "n.unchanged": "unchanged", "n.nonePw": "none",
    "n.found": "{n} networks found.", "n.open": "open",

    "s.update": "Update",
    "s.update.help": "The <code>gauge-cam.bin</code> file of a release, or a full reinstall from <a href=\"https://tdelix.github.io/gauge-cam/\" target=\"_blank\" rel=\"noopener\">the flasher page</a>.",
    "s.updateButton": "Update", "s.backup": "Backup",
    "s.backup.help": "Passwords are not exported.",
    "s.export": "Export the configuration", "s.import": "Import", "s.restart": "Restart",
    "s.version": "Version", "s.id": "Identifier", "s.uptime": "Running for",
    "s.wifi": "Wi-Fi", "s.ap": "Access point", "s.mqtt": "MQTT", "s.photo": "Photo",
    "s.memory": "Free memory", "s.disconnected": "disconnected", "s.closed": "closed",
    "s.connected": "connected", "s.notSet": "not set", "s.none": "none",
    "s.apOn": "{name} on {ip}", "s.kb": "KB",
    "s.chooseFile": "Choose the .bin file first.",
    "s.updated": "Update installed, restarting…",
    "s.updateFailed": "Update failed: {error}", "s.sendFailed": "Upload failed.",
    "s.imported": "Configuration imported.", "s.importedRestart": "Configuration imported, restarting…",
    "s.restarting": "Restarting…",

    "saved": "Saved.", "savedRestart": "Saved. Restarting on the new Wi-Fi network…",
    "unit.s": "{n} s", "unit.min": "{n} min", "unit.h": "{n} h", "unit.d": "{n} d",

    "err.hub": "Needle hub not found near the centre: move the centre or use the fixed mode.",
    "err.contrast": "Needle contrast too low: check the light (LED) or lower the minimum contrast.",
    "err.order": "The points are not in dial order around the centre: check the centre and the values.",
    "err.notConfigured": "Dial to set up: centre, start and end of the scale.",
    "err.band": "Graduations too close to the centre: the centre is probably misplaced.",
    "err.noPhoto": "No photo yet.",
    "err.camera": "Camera error: {error}",
    "err.jump": "Unlikely jump from {from} to {to}: checking again in 2 min ({n}/3).",
  },
};

const LANGS = Object.keys(I18N);
let lang = (() => {
  const saved = localStorage.getItem("lang");
  if (LANGS.includes(saved)) return saved;
  const browser = (navigator.language || "en").slice(0, 2).toLowerCase();
  return LANGS.includes(browser) ? browser : "en";
})();

function t(key, params = {}) {
  const text = I18N[lang][key] ?? I18N.en[key] ?? key;
  return text.replace(/\{(\w+)\}/g, (_, name) => (name in params ? params[name] : `{${name}}`));
}

function applyI18n(root = document) {
  document.documentElement.lang = lang;
  for (const el of root.querySelectorAll("[data-i18n]")) el.textContent = t(el.dataset.i18n);
  for (const el of root.querySelectorAll("[data-i18n-html]")) el.innerHTML = t(el.dataset.i18nHtml);
  for (const el of root.querySelectorAll("[data-i18n-placeholder]")) el.placeholder = t(el.dataset.i18nPlaceholder);
  for (const el of root.querySelectorAll("[data-i18n-title]")) el.title = t(el.dataset.i18nTitle);
}

function setLang(next) {
  if (!LANGS.includes(next)) return;
  lang = next;
  localStorage.setItem("lang", next);
  applyI18n();
}
