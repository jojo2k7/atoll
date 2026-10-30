# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Semi-transparent overlays: drop shadows, light pools, glows, dust."""

from .core import C, WHITE, hexc, alpha, new, px, ellipse, in_ellipse

SHADOW = (30, 18, 40)


def banded(w, h, col, alphas, cx=None, cy=None):
    """Concentric ellipse bands, hard-edged (pixel-art light falloff)."""
    im = new(w, h)
    cx = w / 2 if cx is None else cx
    cy = h / 2 if cy is None else cy
    n = len(alphas)
    for y in range(h):
        for x in range(w):
            d = in_ellipse(x, y, cx, cy, w / 2, h / 2)
            if d > 1:
                continue
            k = min(n - 1, int((1 - d) * n))
            # dither the band edges one pixel deep
            a = alphas[k]
            if k > 0 and (x + y) % 2 == 0 and int((1 - d) * n * 2) % 2 == 0:
                a = alphas[k - 1]
            im.putpixel((x, y), alpha(col, a))
    return im


def shadow(w, h):
    return banded(w, h, SHADOW + (255,), [38, 62, 80])


def rect_shadow(w, h):
    """Soft rectangle (under tables): solid core, dithered rim."""
    im = new(w, h)
    for y in range(h):
        for x in range(w):
            edge = min(x, y, w - 1 - x, h - 1 - y)
            a = 70 if edge >= 2 else (46 if edge == 1 else (28 if (x + y) % 2 == 0 else 0))
            if a:
                im.putpixel((x, y), SHADOW + (a,))
    return im


def pool(w, h, col, strength=1.0):
    al = [int(a * strength) for a in (22, 36, 52, 70)]
    return banded(w, h, col, al)


GLOW = {
    "cool": hexc("#7cc8ff"),
    "green": hexc("#7fe0b0"),
    "warm": hexc("#fff0c8"),
    "red": hexc("#ff6a6a"),
    "violet": hexc("#c8a0ff"),
}
GLOW_ORDER = ["cool", "green", "warm", "red", "violet"]


def monitor_glow(col):
    return banded(44, 30, col, [20, 34, 52], cy=11)


def dust(frame):
    im = new(3, 3)
    a = [60, 140, 200, 140][frame % 4]
    px(im, 1, 1, alpha(WHITE, a))
    if frame % 4 == 2:
        for (x, y) in ((0, 1), (2, 1), (1, 0), (1, 2)):
            px(im, x, y, alpha(WHITE, 50))
    return im


def vignette(w, h):
    im = new(w, h)
    for y in range(h):
        for x in range(w):
            dx = (x - w / 2) / (w / 2)
            dy = (y - h / 2) / (h / 2)
            d = (dx * dx * 0.8 + dy * dy) ** 0.5
            if d > 0.78:
                a = int(min(1, (d - 0.78) / 0.5) * 70)
                a = (a // 14) * 14
                if a:
                    im.putpixel((x, y), SHADOW + (a,))
    return im
