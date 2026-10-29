#!/usr/bin/env python3
"""
uc_make_security_textures.py - paints the two texture pages used by the
security turret and the computer terminal (anim006.all, see
uc_make_security_all.py).

    python uc_make_security_textures.py [output dir]

Writes tex400hi.tga (turret) and tex401hi.tga (terminal), 64 x 64 24-bit
TGAs, the same as the game's own prim pages. They go in
    Release\\server\\textures\\shared\\prims\\
(prim page n is texture page 704 + n, so these are pages 1104 and 1105).

Each page is four 32 x 32 tiles:
    turret   (400):  top-left armour plate   top-right hazard stripes
                     bottom-left barrel steel bottom-right red sensor lens
    terminal (401):  top-left casing         top-right green screen
                     bottom-left keyboard    bottom-right dark plastic
Also writes a 4x preview PNG of each page next to it.
"""

import os
import random
import struct
import sys

from PIL import Image, ImageDraw

SIZE = 64
T = 32   # tile size


def clamp(v):
    return max(0, min(255, int(v)))


def noise_fill(img, box, base, amount, seed):
    rnd = random.Random(seed)
    x0, y0, x1, y1 = box
    px = img.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            n = rnd.randint(-amount, amount)
            px[x, y] = tuple(clamp(c + n) for c in base)


def shade_rect(img, box, light, dark):
    """A bevel: light top/left edge, dark bottom/right edge."""
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    d.line([(x0, y0), (x1, y0)], fill=light)
    d.line([(x0, y0), (x0, y1)], fill=light)
    d.line([(x0, y1), (x1, y1)], fill=dark)
    d.line([(x1, y0), (x1, y1)], fill=dark)


def rivet(img, x, y):
    px = img.load()
    px[x, y] = (200, 205, 210)
    px[x + 1, y] = (150, 155, 160)
    px[x, y + 1] = (120, 125, 130)
    px[x + 1, y + 1] = (60, 62, 66)


# ---------------------------------------------------------------------------
# Turret page
# ---------------------------------------------------------------------------

def turret_page():
    img = Image.new("RGB", (SIZE, SIZE))
    d = ImageDraw.Draw(img)

    # Armour plate: gunmetal with two plates, seams and rivets.
    noise_fill(img, (0, 0, T, T), (92, 100, 108), 6, 1)
    for y in range(T):
        for x in range(T):
            r, g, b = img.getpixel((x, y))
            k = 1.10 - 0.25 * y / T
            img.putpixel((x, y), (clamp(r * k), clamp(g * k), clamp(b * k)))
    d.line([(0, 15), (T - 1, 15)], fill=(40, 44, 48))
    d.line([(0, 16), (T - 1, 16)], fill=(140, 146, 152))
    shade_rect(img, (0, 0, T - 1, T - 1), (150, 156, 162), (36, 40, 44))
    for (x, y) in ((3, 3), (27, 3), (3, 11), (27, 11), (3, 20), (27, 20), (3, 27), (27, 27)):
        rivet(img, x, y)

    # Hazard stripes, yellow and black at 45 degrees, with a dark rim.
    for y in range(T):
        for x in range(T, 2 * T):
            if ((x + y) // 6) % 2 == 0:
                c = (226, 180, 28)
            else:
                c = (28, 28, 30)
            img.putpixel((x, y), c)
    noise = random.Random(2)
    for y in range(T):
        for x in range(T, 2 * T):
            r, g, b = img.getpixel((x, y))
            n = noise.randint(-10, 10)
            img.putpixel((x, y), (clamp(r + n), clamp(g + n), clamp(b + n)))
    shade_rect(img, (T, 0, 2 * T - 1, T - 1), (90, 90, 90), (20, 20, 20))

    # Barrel steel: dark blued metal, lengthwise highlight and ring grooves.
    for y in range(T, 2 * T):
        for x in range(T):
            t = abs(x - 11) / 16.0
            v = 70 - 40 * t
            img.putpixel((x, y), (clamp(v * 0.85), clamp(v * 0.9), clamp(v * 1.05)))
    for x in (6, 7, 18, 19, 28):
        d.line([(x, T), (x, 2 * T - 1)], fill=(18, 20, 24))
    d.line([(10, T), (10, 2 * T - 1)], fill=(150, 156, 170))
    d.line([(11, T), (11, 2 * T - 1)], fill=(110, 116, 130))

    # Sensor lens: black housing, red glass with a glow and a glint.
    d.rectangle([T, T, 2 * T - 1, 2 * T - 1], fill=(16, 16, 18))
    cx, cy = T + 16, T + 16
    for y in range(T, 2 * T):
        for x in range(T, 2 * T):
            r2 = (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2
            if r2 < 13 ** 2:
                k = 1.0 - (r2 ** 0.5) / 13.0
                img.putpixel((x, y), (clamp(110 + 145 * k), clamp(10 + 60 * k * k), clamp(10 + 40 * k * k)))
            elif r2 < 15 ** 2:
                img.putpixel((x, y), (70, 74, 80))
    d.ellipse([cx - 7, cy - 8, cx - 3, cy - 4], fill=(255, 220, 210))
    shade_rect(img, (T, T, 2 * T - 1, 2 * T - 1), (90, 94, 100), (8, 8, 8))

    return img


# ---------------------------------------------------------------------------
# Terminal page
# ---------------------------------------------------------------------------

def terminal_page():
    img = Image.new("RGB", (SIZE, SIZE))
    d = ImageDraw.Draw(img)

    # Casing: light office grey-beige, vent slots and a badge.
    noise_fill(img, (0, 0, T, T), (176, 172, 160), 5, 3)
    for y in range(20, 28, 2):
        d.line([(5, y), (26, y)], fill=(96, 94, 88))
    d.rectangle([4, 4, 13, 9], fill=(52, 70, 120))
    d.line([(6, 6), (11, 6)], fill=(210, 210, 220))
    d.rectangle([20, 5, 22, 7], fill=(60, 220, 90))   # power light
    shade_rect(img, (0, 0, T - 1, T - 1), (220, 216, 204), (100, 98, 90))

    # Screen: green phosphor with text lines and scanlines.
    rnd = random.Random(4)
    for y in range(T):
        for x in range(T, 2 * T):
            fx = (x - T - 15.5) / 16.0
            fy = (y - 15.5) / 16.0
            vign = 1.0 - 0.45 * (fx * fx + fy * fy)
            g = 46 * vign
            img.putpixel((x, y), (clamp(g * 0.25), clamp(g), clamp(g * 0.45)))
    y = 4
    while y < T - 4:
        x = T + 4
        width = rnd.randint(8, 22)
        for xx in range(x, min(x + width, 2 * T - 4)):
            if rnd.random() < 0.82:
                img.putpixel((xx, y), (120, 255, 150))
                img.putpixel((xx, y + 1), (60, 170, 90))
        y += 4
    d.rectangle([T + 4, T - 7, T + 6, T - 5], fill=(170, 255, 190))   # cursor
    for y in range(0, T, 2):
        for x in range(T, 2 * T):
            r, g, b = img.getpixel((x, y))
            img.putpixel((x, y), (clamp(r * 0.78), clamp(g * 0.78), clamp(b * 0.78)))
    shade_rect(img, (T, 0, 2 * T - 1, T - 1), (20, 30, 22), (8, 12, 8))

    # Keyboard: rows of keys.
    d.rectangle([0, T, T - 1, 2 * T - 1], fill=(58, 58, 62))
    for row in range(5):
        ky = T + 2 + row * 6
        for col in range(7):
            kx = 1 + col * 4 + (row % 2)
            if kx + 2 >= T:
                continue
            d.rectangle([kx, ky, kx + 2, ky + 3], fill=(196, 194, 186))
            img.putpixel((kx + 2, ky + 3), (110, 108, 104))
            img.putpixel((kx, ky), (236, 234, 228))
    d.rectangle([6, T + 26, 24, T + 29], fill=(196, 194, 186))      # space bar

    # Dark plastic: bezel / monitor shell.
    noise_fill(img, (T, T, 2 * T, 2 * T), (44, 46, 50), 4, 5)
    for y in range(T + 6, 2 * T - 6, 3):
        d.line([(T + 6, y), (2 * T - 7, y)], fill=(28, 29, 32))
    shade_rect(img, (T, T, 2 * T - 1, 2 * T - 1), (84, 86, 92), (16, 16, 18))

    return img


def save_tga(img, path):
    w, h = img.size
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 0)
    rows = []
    for y in range(h - 1, -1, -1):          # bottom-up, like every TGA the game has
        row = bytearray()
        for x in range(w):
            r, g, b = img.getpixel((x, y))
            row += bytes((b, g, r))
        rows.append(bytes(row))
    with open(path, "wb") as f:
        f.write(header)
        f.write(b"".join(rows))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    for name, img in (("tex400hi", turret_page()), ("tex401hi", terminal_page())):
        save_tga(img, os.path.join(out, name + ".tga"))
        img.resize((SIZE * 4, SIZE * 4), Image.NEAREST).save(os.path.join(out, name + "_preview.png"))
        print("wrote", os.path.join(out, name + ".tga"))


if __name__ == "__main__":
    main()
