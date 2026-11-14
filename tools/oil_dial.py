"""Drawn oil tank level gauge, shaped like a common 0-150 cm cellar indicator.

The pivot is off-centre, the scale is not linear and has no graduation
between 0 and 40 cm, and a red reserve zone starts at 0. The picture gets
uneven light, noise and blur so that it reads like a photo.

    from oil_dial import draw_oil_dial
    image, pivot, points = draw_oil_dial(100)
"""
import math
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

SIZE = (876, 761)
PIVOT = (420, 400)
FACE_CENTRE, FACE_RADIUS = (448, 340), 300
TICK_RADIUS = 200  # where the calibration points are clicked
# Value (cm) -> clock angle of the graduation, degrees.
SCALE = [(0, 300), (40, 335), (50, 345), (60, 356), (70, 368), (80, 380), (90, 393),
         (100, 406), (110, 420), (120, 435), (130, 450), (140, 464), (150, 477)]
RED_ZONE = (0, 20)  # cm


def at(cx, cy, angle, r):
    rad = math.radians(angle)
    return cx + r * math.sin(rad), cy - r * math.cos(rad)


def angle_of(value):
    """Clock angle of a value, interpolated between graduations (may exceed 360)."""
    value = min(max(value, SCALE[0][0]), SCALE[-1][0])
    for (v0, a0), (v1, a1) in zip(SCALE, SCALE[1:]):
        if value <= v1:
            return a0 + (a1 - a0) * (value - v0) / (v1 - v0)
    return SCALE[-1][1]


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:  # Pillow < 10.1
        return ImageFont.load_default()


def draw_oil_dial(level, seed=1):
    """Returns the picture, the pivot and the graduations [(x, y, value)]."""
    rng = random.Random(seed)
    image = Image.new("RGB", SIZE, (128, 118, 104))
    draw = ImageDraw.Draw(image)
    cx, cy = PIVOT
    fx, fy = FACE_CENTRE

    # Housing and face.
    draw.rounded_rectangle((fx - 340, fy - 330, fx + 340, fy + 350), radius=90, fill=(28, 28, 30))
    draw.ellipse((fx - FACE_RADIUS, fy - FACE_RADIUS, fx + FACE_RADIUS, fy + FACE_RADIUS),
                 fill=(236, 234, 226))

    # Red reserve zone, then the graduations (none between 0 and 40).
    a0, a1 = angle_of(RED_ZONE[0]), angle_of(RED_ZONE[1])
    steps = 40
    outer = [at(cx, cy, a0 + (a1 - a0) * i / steps, 228) for i in range(steps + 1)]
    inner = [at(cx, cy, a1 - (a1 - a0) * i / steps, 186) for i in range(steps + 1)]
    draw.polygon(outer + inner, fill=(196, 32, 30))
    points = []
    for value, angle in SCALE:
        major = value % 50 == 0
        draw.line((*at(cx, cy, angle, 186 if major else 190), *at(cx, cy, angle, 226)),
                  fill=(20, 20, 20), width=7 if major else 4)
        x, y = at(cx, cy, angle, TICK_RADIUS)
        points.append((round(x), round(y), value))
    for value in range(45, 150, 10):  # minor ticks
        angle = angle_of(value)
        draw.line((*at(cx, cy, angle, 205), *at(cx, cy, angle, 224)), fill=(40, 40, 40), width=2)
    big = font(38)
    for value, text in ((0, "0"), (50, "50"), (100, "100"), (150, "150")):
        angle = angle_of(value)
        sin, cos = math.sin(math.radians(angle)), math.cos(math.radians(angle))
        if abs(sin) < 0.5:  # above or below the pivot: centred on the tick
            anchor = "mb" if cos > 0 else "mt"
        else:  # beside it: starts or ends at the tick
            anchor = "lm" if sin > 0 else "rm"
        draw.text(at(cx, cy, angle, 236), text, fill=(15, 15, 15), font=big, anchor=anchor)
    draw.text((fx - 10, fy + 190), "FUEL OIL  cm", fill=(70, 70, 70), font=font(22), anchor="mm")

    # Needle, tapered, with a short tail, then the dark hub.
    angle = angle_of(level)
    tip = at(cx, cy, angle, 178)
    tail = at(cx, cy, angle + 180, 34)
    left = at(cx, cy, angle - 90, 7)
    right = at(cx, cy, angle + 90, 7)
    draw.polygon((tip, left, tail, right), fill=(12, 12, 12))
    draw.ellipse((cx - 42, cy - 42, cx + 42, cy + 42), fill=(14, 14, 16))
    draw.ellipse((cx - 9, cy - 9, cx + 9, cy + 9), fill=(70, 70, 74))

    # Photo look: light falling from the upper left, sensor noise, soft focus.
    shade = Image.new("L", SIZE)
    shade.putdata([int(255 * (1.0 - 0.38 * (x / SIZE[0] * 0.6 + y / SIZE[1] * 0.4)))
                   for y in range(SIZE[1]) for x in range(SIZE[0])])
    image = Image.composite(image, Image.new("RGB", SIZE, (0, 0, 0)), shade)
    noise = Image.effect_noise(SIZE, 18).convert("RGB")
    image = Image.blend(image, noise, 0.06)
    image = image.filter(ImageFilter.GaussianBlur(1.2 + rng.random() * 0.2))
    return image, PIVOT, points


if __name__ == "__main__":
    import sys
    draw_oil_dial(float(sys.argv[1]) if len(sys.argv) > 1 else 100)[0].save(
        sys.argv[2] if len(sys.argv) > 2 else "oil-dial.png")
