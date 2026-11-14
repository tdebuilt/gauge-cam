#!/usr/bin/env python3
"""Builds a-plus.html: direction B, round 1 (first set-up path).

    python3 maquette/propositions/build_aplus.py
"""
import json
import os
import re

from build import HERE, H, POINTS, W, fill, read

G_TOTAL = 25  # checked by the gesture walk (see the session notes)

STEPS = dict(re.findall(r"<!--step:([a-z-]+)-->\n(.*?)(?=<!--step:|\Z)", read("steps.html"), re.S))
NAMES = ["Photo", "Cadrage", "Graduations", "Vérification", "Cuve", "Home Assistant"]


def prog(n):
    out = ['<div class="prog">']
    for i, name in enumerate(NAMES, 1):
        cls = "done" if i < n else "cur" if i == n else ""
        out.append(f'<span class="{cls}"><i></i>{name if i == n else ""}</span>')
    out.append("</div>")
    return "".join(out)


def steps_html(names):
    text = "".join(STEPS[n] for n in names)
    return re.sub(r"\{\{PROG:(\d)\}\}", lambda m: prog(int(m.group(1))), text)


def frame(start, names, cls="", url="gauge-cam-3f2a1c.local", bubble=False, inner=None):
    body = inner if inner is not None else steps_html(names)
    extra = ('<div class="bubble" data-bubble hidden><input inputmode="decimal" placeholder="valeur">'
             '<button class="btn pri sm" data-bubble-ok>OK</button></div>') if bubble else ""
    return (f'<div class="frame"><div class="bar"><i></i><i></i><i></i><span>{url}</span></div>'
            f'<div class="fit"><div class="vp dB {cls}" data-w="1200" data-h="760" data-start="{start}">'
            f'{body}{extra}</div></div></div>')


FLASHER = """<div class="screen fB" data-screen="fl" style="background:#0e1114">
  <div class="fhd">{{LOGO}}<b>GAUGE CAM</b><div class="gh"><span>Documentation</span><span>GitHub</span><span>FR · EN</span></div></div>
  <div class="hero"><div><div class="kick">ESP32-CAM · MQTT · Home Assistant</div><h1>Votre jauge de mazout, lue toute seule.</h1>
    <button class="inst">Installer sur l'ESP32-CAM</button><span class="ver">v0.1.0</span></div><div>{{GAUGE_B_HERO}}</div></div>
  <div class="ewt-dim"></div>
  <div class="ewt"><h4>Gauge Cam</h4><p>Device connected to the network!</p><p>v0.1.0 · Wi-Fi « maison »</p>
    <div class="acts"><span class="main">Visit device</span><span>Change Wi-Fi</span><span>Logs &amp; console</span><span>Close</span></div></div>
</div>"""

BLOCKS = [
    ("open", "Ouvrir l'appareil depuis la page d'installation",
     "Après l'installation et le Wi-Fi, le bouton « Visit device » de la page d'installation ouvre directement le premier écran de la mise en service.",
     "Rien à coder : le firmware donne déjà son adresse à la page (Improv). Cette boîte est celle d'esp-web-tools, en anglais et non modifiable ; texte approximatif. "
     "Sans Wi-Fi, le point d'accès gauge-cam-xxxxxx ouvre le même parcours.",
     [frame("fl", [], url="tdebuilt.github.io/gauge-cam/", inner=FLASHER)]),
    ("steps", "Six étapes annoncées, chacune remise à plus tard",
     "Le premier écran annonce les six étapes et leur durée ; « Plus tard » reste possible à chaque étape.",
     "Le Wi-Fi n'est plus une étape : il est réglé par la page d'installation ou le point d'accès.",
     [frame("welcome", ["welcome", "photo", "home-todo"])]),
    ("photo", "Photo : flash et rotation seulement",
     "L'étape photo ne montre que le flash, la rotation et le miroir ; résolution, exposition et contraste passent sous « Plus de réglages ».",
     "Chaque réglage reprend une photo, comme aujourd'hui.",
     [frame("photo", ["photo", "frame"])]),
    ("frame", "Cadrage en un geste, centre trouvé seul",
     "Un cadre à ajuster autour du cadran ; le centre de l'aiguille est trouvé tout seul et se corrige d'un clic.",
     "Le firmware suit déjà le moyeu noir : c'est le réglage par défaut.",
     [frame("frame", ["frame", "grad"])]),
    ("grad", "Graduations dans l'ordre, sans rien taper",
     "On choisit un modèle de jauge, puis on clique les graduations dans l'ordre ; la valeur suivante est proposée seule. "
     "La bulle ne sert qu'aux points en plus.",
     "13 clics au lieu de 13 clics et 13 saisies. Le firmware ne change pas : l'interface envoie les mêmes points. "
     "Clique sur la photo, ou « Placer les autres (démo) ».",
     [frame("grad", ["grad", "check"], bubble=True)]),
    ("check", "Vérifier la valeur lue",
     "Avant la cuve, l'appareil fait une lecture et demande si la valeur est juste ; « Non » ramène aux graduations.",
     "La lecture d'essai existe déjà (bouton Tester).",
     [frame("check", ["check", "grad", "tank"])]),
    ("tank", "Cuve : forme et capacité de la plaque",
     "La cuve se décrit par sa forme et la capacité inscrite sur la plaque ; la hauteur est proposée (fin de l'échelle). "
     "Les dimensions restent possibles.",
     "Les dimensions sont calculées par l'interface : le firmware ne change pas. Choisis une forme et tape une capacité.",
     [frame("tank", ["tank", "ha"])]),
    ("ha", "Home Assistant : trois champs et un test",
     "Serveur, utilisateur, mot de passe, puis « Tester et terminer », qui ne valide qu'une fois la connexion réussie.",
     "La découverte Home Assistant est active par défaut ; les autres réglages MQTT restent dans Réglages.",
     [frame("ha", ["ha", "done"])]),
    ("todo", "La fin, ou ce qui manque",
     "Une fois tout fait, l'accueil dit où trouver l'appareil dans Home Assistant. Si une étape a été remise à plus tard, "
     "la liste « Encore 2 étapes » prend sa place, avec un bouton par étape.",
     "À gauche : tout est fait. À droite : cuve et Home Assistant remis à plus tard.",
     [frame("done", ["done"]), frame("home-todo", ["home-todo", "tank", "ha"])]),
    ("theme", "Thème sombre ou clair selon l'appareil",
     "Le thème suit le réglage de l'ordinateur ou du téléphone : sombre (à gauche) ou clair (à droite).",
     "Il n'y a pas de bouton pour forcer un thème. On peut en ajouter un dans Réglages si tu le veux.",
     [frame("done", ["done"]), frame("done", ["done"], cls="light")]),
]


def blocks():
    out = []
    for n, (k, title, decision, why, frames) in enumerate(BLOCKS, 1):
        two = " two" if len(frames) == 2 else ""
        q = (f'<div><h3 class="q">{n}. {title}</h3><p>{decision}</p><p class="why">{why}</p>'
             f'<div class="ans"><button data-a="Oui">Oui</button><button data-a="À revoir">À revoir</button>'
             f'<button data-a="Non">Non</button></div><textarea placeholder="Remarque (facultatif)"></textarea></div>')
        out.append(f'<div class="blk{two}" data-k="{k}" data-n="{n}">{"".join(f"<div>{f}</div>" for f in frames)}{q}</div>')
    return "\n".join(out)


def main():
    b_css = re.search(r"<style>(.*?)</style>", read("b.html"), re.S).group(1)
    page = read("aplus.html")
    page = page.replace("{{B_CSS}}", b_css)
    page = page.replace("{{ALL_STEPS}}", steps_html(["welcome", "photo", "frame", "grad", "check", "tank", "ha", "done", "home-todo"]))
    page = page.replace("{{BLOCKS}}", blocks())
    page = page.replace("{{G_TOTAL}}", str(G_TOTAL))
    page = page.replace("{{POINTS_PCT}}", json.dumps([[round(x / W * 100, 2), round(y / H * 100, 2)] for x, y, _ in POINTS]))
    page = page.replace("{{FONTS}}", read("fonts.css"))
    page = fill(page)
    left = re.findall(r"\{\{[A-Z_:]+[^}]*\}\}", page)
    if left:
        raise SystemExit(f"unfilled slots: {sorted(set(left))[:10]}")
    with open(os.path.join(HERE, "a-plus.html"), "w", encoding="utf-8") as f:
        f.write(page)
    print(f"a-plus.html {len(page) // 1024} KB")


if __name__ == "__main__":
    main()
