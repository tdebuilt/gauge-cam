#!/usr/bin/env python3
"""Writes the two web fonts of the interface, cut down to Latin-1.

    python3 -m venv /tmp/font-venv && /tmp/font-venv/bin/pip install fonttools brotli
    /tmp/font-venv/bin/python tools/subset_fonts.py

Sources: the fonts embedded in maquette/propositions/fonts.css: IBM Plex Mono,
renamed Gauge Mono once cut, and Manrope (SIL Open Font License 1.1, see
web/fonts-license.txt). Body text uses the system font, so the device carries
two small files only.
"""
import base64
import io
import os
import re

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "maquette", "propositions", "fonts.css")
FONTS = {("IBM Plex Mono", "500"): "gaugemono.woff2", ("Manrope", "800"): "manrope.woff2"}
# "Plex" is a Reserved Font Name: a modified (subset) font must not carry it.
RENAME = {"IBM Plex Mono": "Gauge Mono", "IBMPlexMono": "GaugeMono"}
# Latin-1 plus the punctuation the French and English texts use. The sources
# have no narrow no-break space: app.js swaps it for U+00A0 in numbers.
UNICODES = [*range(0x20, 0x7F), *range(0xA0, 0x100), 0x2009, 0x2013, 0x2014, 0x2019, 0x201C, 0x201D,
            0x2026, 0x202F, 0x2192, 0x2212, 0x2248]


def main():
    css = open(SOURCE, encoding="utf-8").read()
    for block in re.findall(r"@font-face\s*\{(.*?)\}", css, re.S):
        family = re.search(r"font-family:\s*'([^']+)'", block).group(1)
        weight = re.search(r"font-weight:\s*(\d+)", block).group(1)
        name = FONTS.get((family, weight))
        if not name:
            continue
        data = base64.b64decode(re.search(r"base64,([A-Za-z0-9+/=]+)", block).group(1))
        font = TTFont(io.BytesIO(data))
        options = subset.Options()
        options.flavor = "woff2"
        options.layout_features = ["kern", "liga", "tnum"]
        options.name_IDs = [0, 1, 2, 13, 14]  # keeps the copyright and licence names
        subsetter = subset.Subsetter(options)
        subsetter.populate(unicodes=UNICODES)
        subsetter.subset(font)
        for record in font["name"].names:
            if record.nameID in (1, 3, 4, 6, 16, 17, 21):
                text = record.toUnicode()
                for old, new in RENAME.items():
                    text = text.replace(old, new)
                record.string = text
        out = os.path.join(ROOT, "web", name)
        font.flavor = "woff2"
        font.save(out)
        print(f"{name}: {os.path.getsize(out)} bytes")


if __name__ == "__main__":
    main()
