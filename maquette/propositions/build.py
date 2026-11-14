#!/usr/bin/env python3
"""Builds index.html: four design directions for the device interface and
the flasher page. Self-contained and offline (fonts and photo inlined).

    python3 maquette/propositions/build.py

Inputs: page.html, a.html .. d.html (templates with {{KEY}} slots),
fonts.css (base64 @font-face blocks) and geo.json (drawn dial photo and its
calibration points, made from tools/oil_dial.py).
"""
import json
import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def read(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        return f.read()


geo = json.loads(read("geo.json"))
W, H = geo["w"], geo["h"]
PX, PY = geo["pivot"]
TX, TY = geo["tip"]
POINTS = geo["points"]


def photo_overlay(kind, accent="#ffb020", label=None):
    """SVG drawn over the dial photo, in photo pixels."""
    out = [f'<svg class="ov" viewBox="0 0 {W} {H}" preserveAspectRatio="xMidYMid slice">']
    labels = kind == "calibl"
    if labels:
        kind = "calib"
    if kind == "calib":
        out.append(f'<rect x="70" y="40" width="{W - 140}" height="{H - 60}" fill="none" stroke="#ffc83d" '
                   f'stroke-width="2" stroke-dasharray="7 5"/>')
        out.append(f'<path d="M{PX - 9} {PY}h18M{PX} {PY - 9}v18" stroke="#ff4d4d" stroke-width="2.5"/>')
        for i, (x, y, v) in enumerate(POINTS):
            colour = "#2ecc71" if i == 0 else "#d65cff" if i == len(POINTS) - 1 else accent
            out.append(f'<circle cx="{x}" cy="{y}" r="5" fill="{colour}" stroke="#111" stroke-width="1.5"/>')
            if labels and v in (0, 50, 100, 150):
                dx, dy = x - PX, y - PY
                n = math.hypot(dx, dy)
                lx, ly = x - dx / n * 19, y - dy / n * 19
                out.append(f'<g transform="translate({lx:.1f} {ly:.1f})"><rect x="-12" y="-7" width="24" height="14" rx="7" '
                           f'fill="rgba(10,14,20,.8)"/><text y="3.3" text-anchor="middle" fill="#fff" font-size="9" '
                           f'font-family="Inter,sans-serif">{v}</text></g>')
    if kind in ("live", "calib"):
        out.append(f'<line x1="{PX}" y1="{PY}" x2="{TX}" y2="{TY}" stroke="{accent}" stroke-width="4" '
                   f'stroke-linecap="round" opacity=".9"/>')
        out.append(f'<circle cx="{PX}" cy="{PY}" r="6" fill="{accent}"/>')
    if label:
        out.append(f'<g transform="translate({TX + 10} {TY - 34})"><rect width="92" height="30" rx="15" '
                   f'fill="rgba(10,14,20,.78)" stroke="{accent}"/><text x="46" y="20" text-anchor="middle" '
                   f'fill="#fff" font-size="15" font-weight="600" font-family="Inter,sans-serif">{label}</text></g>')
    out.append("</svg>")
    return "".join(out)


def photo(cls="", kind="none", accent="#ffb020", label=None, pick=False):
    attrs = f' data-pick="{pick}"' if pick else ""
    return f'<div class="ph {cls}"{attrs}>{photo_overlay(kind, accent, label)}<div class="marks"></div></div>'


def polar(cx, cy, r, deg):
    rad = math.radians(deg)
    return cx + r * math.sin(rad), cy - r * math.cos(rad)


def arc(cx, cy, r, a0, a1):
    x0, y0 = polar(cx, cy, r, a0)
    x1, y1 = polar(cx, cy, r, a1)
    large = 1 if a1 - a0 > 180 else 0
    return f"M{x0:.1f} {y0:.1f}A{r} {r} 0 {large} 1 {x1:.1f} {y1:.1f}"


def gauge_b(percent, size=300, stroke=18, needle=False):
    """Instrument arc, 240 degrees, red reserve zone at the start."""
    c = size / 2
    r = c - stroke
    start, sweep = -120, 240
    value = start + sweep * percent / 100
    ticks = []
    for i in range(0, 11):
        a = start + sweep * i / 10
        x0, y0 = polar(c, c, r - stroke * 0.9, a)
        x1, y1 = polar(c, c, r - stroke * (1.6 if i % 5 == 0 else 1.3), a)
        ticks.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}"/>')
    nx, ny = polar(c, c, r - stroke * 2.2, value)
    return (f'<svg class="gauge" viewBox="0 0 {size} {size}">'
            f'<path d="{arc(c, c, r, start, start + sweep)}" class="g-track" stroke-width="{stroke}" fill="none" stroke-linecap="round"/>'
            f'<path d="{arc(c, c, r, start, value)}" class="g-val" stroke-width="{stroke}" fill="none" stroke-linecap="round"/>'
            f'<path d="{arc(c, c, r, start, start + sweep * 0.12)}" class="g-red" stroke-width="{stroke}" fill="none" stroke-linecap="round"/>'
            f'<g class="g-ticks">{"".join(ticks)}</g>'
            + (f'<line x1="{c}" y1="{c}" x2="{nx:.1f}" y2="{ny:.1f}" class="g-needle"/>'
               f'<circle cx="{c}" cy="{c}" r="9" class="g-hub"/>' if needle else '')
            + '</svg>')


def tank_c(percent, w=320, h=170):
    """Horizontal cylinder seen from the side, filled to the level."""
    r = h / 2 - 6
    level_y = 6 + (h - 12) * (1 - percent / 100)
    body = f"M{r + 6} 6H{w - r - 6}A{r} {r} 0 0 1 {w - r - 6} {h - 6}H{r + 6}A{r} {r} 0 0 1 {r + 6} 6Z"
    return (f'<svg class="tank" viewBox="0 0 {w} {h}"><defs><clipPath id="tk"><path d="{body}"/></clipPath></defs>'
            f'<path d="{body}" class="t-body"/>'
            f'<g clip-path="url(#tk)"><rect x="0" y="{level_y:.1f}" width="{w}" height="{h}" class="t-oil"/>'
            f'<path d="M0 {level_y:.1f}q20 -5 40 0t40 0t40 0t40 0t40 0t40 0t40 0t40 0t40 0" class="t-wave"/></g>'
            f'<path d="{body}" class="t-line"/></svg>')


def history(w=520, h=120, days=14):
    """Fictive daily volumes: slow use, one delivery."""
    vols = []
    v = 1450
    for d in range(days * 4 + 1):
        if d == 22:
            v = 3350
        vols.append(v)
        v -= 9.5 + (4 if d % 7 in (5, 6) else 0)
    lo, hi = 0, 4418
    pts = [(w * i / (len(vols) - 1), h - 8 - (h - 16) * (x - lo) / (hi - lo)) for i, x in enumerate(vols)]
    line = "M" + "L".join(f"{x:.1f} {y:.1f}" for x, y in pts)
    area = line + f"L{w} {h}L0 {h}Z"
    return (f'<svg class="hist" viewBox="0 0 {w} {h}" preserveAspectRatio="none">'
            f'<path d="{area}" class="h-area"/><path d="{line}" class="h-line" fill="none"/></svg>')


def shape_icons():
    return {
        "ICON_BOX": '<svg viewBox="0 0 48 36"><path d="M6 10l10-6h26v22l-10 6H6z M6 10h26v22 M32 10l10-6" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"/></svg>',
        "ICON_HCYL": '<svg viewBox="0 0 48 36"><path d="M12 6h24a12 12 0 0 1 0 24H12a12 12 0 0 1 0-24z" fill="none" stroke="currentColor" stroke-width="2.2"/><path d="M12 6a5 12 0 0 1 0 24" fill="none" stroke="currentColor" stroke-width="1.6" opacity=".6"/></svg>',
        "ICON_VCYL": '<svg viewBox="0 0 48 36"><ellipse cx="24" cy="7" rx="11" ry="4" fill="none" stroke="currentColor" stroke-width="2.2"/><path d="M13 7v22a11 4 0 0 0 22 0V7" fill="none" stroke="currentColor" stroke-width="2.2"/></svg>',
        "ICON_TABLE": '<svg viewBox="0 0 48 36"><path d="M8 30L18 22L28 14L40 6" fill="none" stroke="currentColor" stroke-width="2.2"/><path d="M6 4v28h38" fill="none" stroke="currentColor" stroke-width="1.6" opacity=".6"/></svg>',
    }


LOGO = ('<svg class="logo" viewBox="0 0 32 32"><circle cx="16" cy="16" r="13" fill="none" stroke="currentColor" stroke-width="2.4"/>'
        '<path d="M8.5 21A9 9 0 0 1 16 7" fill="none" stroke="#e5484d" stroke-width="2.4" stroke-linecap="round"/>'
        '<path d="M16 16l6-6" stroke="currentColor" stroke-width="2.6" stroke-linecap="round"/><circle cx="16" cy="16" r="2.6" fill="currentColor"/></svg>')

SLOTS = {
    "LOGO": LOGO,
    "PHOTO_IMG": geo["img"],
    "GAUGE_B": gauge_b(70.5),
    "GAUGE_B_HERO": gauge_b(70.5, 340, 20, needle=True).replace('class="gauge"', 'class="gauge hero-gauge"'),
    "TANK_C": tank_c(66.4),
    "HIST_D": history(),
    "HIST_D_SMALL": history(240, 44),
    **shape_icons(),
}


def photos(text):
    """{{PHOTO:kind:accent:label:pick:cls}} slots."""
    def repl(m):
        kind, accent, label, pick, cls = (m.group(1).split(":") + [""] * 5)[:5]
        pick = {"pick": "1", "always": "always", "seq": "seq"}.get(pick, "")
        return photo(cls, kind or "none", accent or "#ffb020", label or None, pick)
    return re.sub(r"\{\{PHOTO:([^}]*)\}\}", repl, text)


def fill(text):
    text = photos(text)
    for key, value in SLOTS.items():
        text = text.replace("{{" + key + "}}", value)
    return text


def main():
    page = read("page.html")
    for part in "abcd":
        page = page.replace("{{DIR_" + part.upper() + "}}", read(part + ".html"))
    page = page.replace("{{FONTS}}", read("fonts.css"))
    page = fill(page)
    left = re.findall(r"\{\{[A-Z_:]+[^}]*\}\}", page)
    if left:
        raise SystemExit(f"unfilled slots: {sorted(set(left))[:10]}")
    with open(os.path.join(HERE, "index.html"), "w", encoding="utf-8") as f:
        f.write(page)
    print(f"index.html {len(page) // 1024} KB")


if __name__ == "__main__":
    main()
