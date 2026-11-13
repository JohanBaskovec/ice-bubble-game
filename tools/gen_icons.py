#!/usr/bin/env python3
"""Genere les icones de l'application (Android, iOS, Web, fiche Play Store).

Usage : python3 tools/gen_icons.py      (depuis la racine du depot, necessite Pillow)
Le dessin reprend la boule de Noel bleue du jeu sur fond de nuit.
"""
import math
import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 1024  # taille de travail


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def draw_icon(size, rounded):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bg = Image.new("RGBA", (S, S))
    px = bg.load()
    top, bottom = (10, 20, 48), (40, 70, 130)
    for y in range(S):
        c = lerp(top, bottom, y / S) + (255,)
        for x in range(S):
            px[x, y] = c
    d = ImageDraw.Draw(bg)
    # neige au sol
    d.ellipse((-200, 820, S + 200, 1400), fill=(236, 247, 255, 255))
    # flocons
    for (fx, fy, fr) in [(170, 200, 10), (840, 160, 8), (900, 420, 6), (120, 520, 7), (760, 760, 9), (260, 760, 6)]:
        d.ellipse((fx - fr, fy - fr, fx + fr, fy + fr), fill=(255, 255, 255, 220))
    # boule
    cx, cy, r = 512, 560, 300
    ball = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bp = ball.load()
    hx, hy = cx - r * 0.35, cy - r * 0.4
    c_in, c_mid, c_out = (255, 255, 255), (134, 204, 255), (26, 85, 170)
    for y in range(cy - r, cy + r):
        for x in range(cx - r, cx + r):
            dx, dy = x - cx, y - cy
            if dx * dx + dy * dy > r * r:
                continue
            t = math.hypot(x - hx, y - hy) / (r * 1.35)
            t = min(1.0, t)
            col = lerp(c_in, c_mid, t / 0.2) if t < 0.2 else lerp(c_mid, c_out, (t - 0.2) / 0.8)
            bp[x, y] = col + (255,)
    bd = ImageDraw.Draw(ball)
    # bandeau en pointilles
    for i in range(6):
        a0 = math.pi * (i + 0.2) / 6
        a1 = math.pi * (i + 0.75) / 6
        pts = [(cx + math.cos(a0 + (a1 - a0) * k / 8) * r * 0.9, cy + r * 0.15 + math.sin(a0 + (a1 - a0) * k / 8) * r * 0.32)
               for k in range(9)]
        bd.line(pts, fill=(255, 255, 255, 170), width=int(r * 0.12))
    bg.alpha_composite(ball)
    # attache doree
    d.rectangle((cx - r * 0.26, cy - r * 1.2, cx + r * 0.26, cy - r * 0.92), fill=(217, 178, 74, 255))
    d.ellipse((cx - r * 0.16, cy - r * 1.46, cx + r * 0.16, cy - r * 1.14), outline=(217, 178, 74, 255), width=int(r * 0.08))
    # flocon blanc sur la boule
    fx, fy, fr = cx + 20, cy + 10, 110
    for k in range(6):
        a = k * math.pi / 3
        ex, ey = fx + math.cos(a) * fr, fy + math.sin(a) * fr
        d.line((fx, fy, ex, ey), fill=(255, 255, 255, 235), width=18)
        mx, my = fx + math.cos(a) * fr * 0.55, fy + math.sin(a) * fr * 0.55
        for s in (-1, 1):
            b = a + s * 0.7
            d.line((mx, my, mx + math.cos(b) * fr * 0.35, my + math.sin(b) * fr * 0.35), fill=(255, 255, 255, 235), width=14)

    if rounded:
        mask = Image.new("L", (S, S), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), radius=int(S * 0.22), fill=255)
        img.paste(bg, (0, 0), mask)
    else:
        img = bg
    return img.resize((size, size), Image.LANCZOS)


def save(img, rel, rgb=False):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if rgb:
        img = img.convert("RGB")  # l'App Store refuse la transparence
    img.save(path)
    print("ecrit", rel)


def main():
    master = draw_icon(1024, rounded=False)
    save(master, "platforms/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", rgb=True)
    save(draw_icon(512, rounded=False), "assets/icon/icon-512.png", rgb=True)  # fiche Play Store
    for name, px in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]:
        save(draw_icon(px, rounded=True), f"platforms/android/app/src/main/res/mipmap-{name}/ic_launcher.png")
    save(draw_icon(64, rounded=True), "platforms/web/favicon.png")


if __name__ == "__main__":
    main()
