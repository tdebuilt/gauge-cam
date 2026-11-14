#!/usr/bin/env python3
"""Write the host test fixtures: PPM images and cases.txt.

    python3 tools/make_fixtures.py build/fixtures

Each line of cases.txt:
    name file roi_x roi_y roi_w roi_h mode pivot_x pivot_y expected tolerance n x y value ...
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oil_dial import draw_oil_dial  # noqa: E402

PIVOT_CLICK_OFFSET = (6, -5)  # a rough click, the hub mode refines it


def at(cx, cy, angle, r):
    rad = math.radians(angle)
    return cx + r * math.sin(rad), cy - r * math.cos(rad)


def synthetic_dial(needle_value, clockwise=True, size=400, pivot=(170, 200), red_zone=True):
    """Off-centre pivot, 0-100 over 240 degrees, ticks, numerals, red reserve zone."""
    image = Image.new("RGB", (size, size), (90, 90, 90))
    draw = ImageDraw.Draw(image)
    draw.ellipse((20, 20, size - 20, size - 20), fill=(225, 225, 220))
    cx, cy = pivot
    start, span = 240.0, 240.0
    direction = 1 if clockwise else -1
    angle_of = lambda v: (start + direction * span * v / 100) % 360
    if red_zone:  # red band 0-20 at the graduations, like a reserve zone
        for v in [x / 4 for x in range(0, 81)]:
            draw.line((*at(cx, cy, angle_of(v), 128), *at(cx, cy, angle_of(v), 152)), fill=(200, 30, 30), width=6)
    points = []
    for value in range(0, 101, 10):
        a = angle_of(value)
        draw.line((*at(cx, cy, a, 130), *at(cx, cy, a, 150)), fill=(20, 20, 20), width=4)
        x, y = at(cx, cy, a, 140)
        points.append((round(x), round(y), value))
        draw.text(at(cx, cy, a, 160), str(value), fill=(0, 0, 0))
    # Coloured mark inside the needle band must not attract the needle search.
    red = angle_of(75)
    draw.line((*at(cx, cy, red, 60), *at(cx, cy, red, 110)), fill=(210, 30, 30), width=8)
    a = angle_of(needle_value)
    draw.line((cx, cy, *at(cx, cy, a, 125)), fill=(15, 15, 15), width=6)
    draw.ellipse((cx - 22, cy - 22, cx + 22, cy + 22), fill=(10, 10, 10))
    return image, points


def degrade(image, width=None, brightness=1.0, blur=0.0):
    if width:
        image = image.resize((width, round(image.height * width / image.width)))
    if brightness != 1.0:
        image = ImageEnhance.Brightness(image).enhance(brightness)
    if blur:
        image = image.filter(ImageFilter.GaussianBlur(blur))
    return image


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    lines = []

    def case(name, image, roi, mode, pivot, expected, tol, points):
        path = os.path.join(out_dir, name + ".ppm")
        image.convert("RGB").save(path)
        pts = " ".join(f"{x:g} {y:g} {v:g}" for x, y, v in points)
        lines.append(f"{name} {path} {' '.join(map(str, roi))} {mode} {pivot[0]:g} {pivot[1]:g} "
                     f"{expected:g} {tol:g} {len(points)} {pts}")

    # Drawn oil tank gauge: off-centre pivot, nothing between 0 and 40, red zone 0-20.
    oil, pivot, oil_points = draw_oil_dial(100)
    click = (pivot[0] + PIVOT_CLICK_OFFSET[0], pivot[1] + PIVOT_CLICK_OFFSET[1])
    case("oil_hub", oil, (0, 0, 0, 0), "hub", click, 100, 2, oil_points)
    case("oil_fixed", oil, (0, 0, 0, 0), "fixed", pivot, 100, 2, oil_points)
    for level in (0, 8, 15, 27, 40, 63, 118, 150):  # red zone and the stretch without graduations
        img, _, _ = draw_oil_dial(level, seed=level)
        case(f"oil_{level}", img, (0, 0, 0, 0), "hub", click, level, 2, oil_points)

    # The dial inside a larger frame, read through a region of interest.
    ox, oy = 180, 140
    framed = Image.new("RGB", (oil.width + 400, oil.height + 300), (70, 66, 60))
    framed.paste(oil, (ox, oy))
    shifted = [(x + ox, y + oy, v) for x, y, v in oil_points]
    case("oil_roi", framed, (ox - 20, oy - 20, oil.width + 40, oil.height + 40), "hub",
         (click[0] + ox, click[1] + oy), 100, 2, shifted)

    for name, args in {
        "320px": dict(width=320), "200px": dict(width=200), "dark": dict(brightness=0.35),
        "blur": dict(blur=3), "combo": dict(width=320, brightness=0.4, blur=1.5),
    }.items():
        img = degrade(oil, **args)
        f = img.width / oil.width
        pts = [(x * f, y * f, v) for x, y, v in oil_points]
        case(f"oil_{name}", img, (0, 0, 0, 0), "hub", (click[0] * f, click[1] * f), 100, 4, pts)

    for clockwise in (True, False):
        for value in (0, 4, 12, 19, 33, 50, 75, 92, 100):  # 0-20 is in the red zone
            img, pts = synthetic_dial(value, clockwise)
            tag = "cw" if clockwise else "ccw"
            case(f"synth_{tag}_{value}", img, (0, 0, 0, 0), "hub", (176, 195), value, 1, pts)
    img, pts = synthetic_dial(42)
    case("synth_fixed_42", img, (0, 0, 0, 0), "fixed", (170, 200), 42, 1, pts)

    with open(os.path.join(out_dir, "cases.txt"), "w") as handle:
        handle.write("\n".join(lines) + "\n")
    print(f"{len(lines)} cases written to {out_dir}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "fixtures"))
