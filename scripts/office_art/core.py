# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Palette, pixel helpers and the sprite-sheet registry."""

from pathlib import Path
import random

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "data" / "office"
FONT = ROOT / "data" / "fonts" / "monogram-extended.ttf"


def hexc(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


# One deliberate palette. Ramps run dark -> light; shadows lean to plum/blue,
# highlights lean to warm yellow.
RAMPS = {
    "ink":    ["#1e1724", "#2b2033", "#3d2f45", "#54435a"],
    "wood":   ["#4a2c25", "#6b3f2f", "#8f5a3c", "#b47a4c", "#d39d63", "#ecc489"],
    "oak":    ["#5a3428", "#7f4e34", "#a0663f", "#bd8250", "#d6a066"],
    "walnut": ["#3a2224", "#563232", "#74463b", "#94604a", "#b37d5c"],
    "plank":  ["#5d3a30", "#7d4f3a", "#9a6544", "#b07650", "#c48a5c", "#d6a06c"],
    "cream":  ["#a98d7c", "#c8ac92", "#dcc4a4", "#ebd8b8", "#f6ead0"],
    "sage":   ["#2c4440", "#3b5a50", "#4f7462", "#6a9178", "#8db296"],
    "green":  ["#1d3a35", "#245041", "#2f6b45", "#4a8c4a", "#78b155", "#b6d76d"],
    "teal":   ["#173640", "#1f5257", "#2a7470", "#3d9a8a", "#6cc3a8"],
    "blue":   ["#1b2340", "#24365e", "#2f5288", "#4277b4", "#6ea4d6", "#a9d3ee"],
    "sky":    ["#5d8fc7", "#7fb0dc", "#a8d0ec", "#d4ecf6"],
    "red":    ["#4a1a2a", "#7a2434", "#a8343a", "#d0503f", "#ec7f5e"],
    "orange": ["#7a3326", "#b0532c", "#d97b36", "#f0a24a", "#f8c672"],
    "yellow": ["#8a6224", "#c08e2c", "#e8bc3e", "#f6dc6a", "#fcf0a8"],
    "pink":   ["#6e2b4a", "#a3456a", "#d06b88", "#ec9aab", "#f8c8c8"],
    "purple": ["#2e2147", "#47326b", "#644b90", "#8a6db4", "#b59bd6"],
    "steel":  ["#1f2330", "#2c3242", "#3e4659", "#566075", "#7a8496", "#a6aebb", "#d2d7de", "#f0f2f4"],
    "skin0":  ["#a8664e", "#d18c6c", "#f0b48e", "#fbd5b4"],
    "skin1":  ["#8a4e36", "#b46e48", "#d69464", "#ecb788"],
    "skin2":  ["#5c3024", "#7e4630", "#a3633f", "#c2845a"],
    "skin3":  ["#3e2119", "#56301f", "#74452c", "#93613f"],
    "screen": ["#0e1420", "#152033", "#1d2c45"],
    "code":   ["#7fd8ae", "#f2c45e", "#7cb8f0", "#f08a8a", "#c8a6f0", "#d8dee8"],
}
C = {k: [hexc(c) for c in v] for k, v in RAMPS.items()}
T = (0, 0, 0, 0)
OUTLINE = C["ink"][0]
WHITE = hexc("#f6f2ea")


def alpha(c, a):
    return (c[0], c[1], c[2], a)


def new(w, h):
    return Image.new("RGBA", (w, h), T)


def px(im, x, y, c):
    if c is None:
        return
    if 0 <= x < im.width and 0 <= y < im.height:
        if len(c) == 4 and c[3] < 255 and c[3] > 0:
            im.putpixel((x, y), blend(im.getpixel((x, y)), c))
        else:
            im.putpixel((x, y), c)


def blend(dst, src):
    a = src[3] / 255.0
    if dst[3] == 0:
        return src
    out = tuple(int(round(src[i] * a + dst[i] * (1 - a))) for i in range(3))
    return out + (max(dst[3], src[3]),)


def rect(im, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            px(im, xx, yy, c)


def hline(im, x, y, w, c):
    rect(im, x, y, w, 1, c)


def vline(im, x, y, h, c):
    rect(im, x, y, 1, h, c)


def frame_rect(im, x, y, w, h, c):
    hline(im, x, y, w, c)
    hline(im, x, y + h - 1, w, c)
    vline(im, x, y, h, c)
    vline(im, x + w - 1, y, h, c)


def line(im, x0, y0, x1, y1, c):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        px(im, x0, y0, c)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def ellipse(im, cx, cy, rx, ry, c):
    """Filled pixel ellipse; cx/cy may be half-integers."""
    for y in range(int(cy - ry - 1), int(cy + ry + 2)):
        for x in range(int(cx - rx - 1), int(cx + rx + 2)):
            dx = (x + 0.5 - cx) / max(rx, 0.01)
            dy = (y + 0.5 - cy) / max(ry, 0.01)
            if dx * dx + dy * dy <= 1.0:
                px(im, x, y, c)


def in_ellipse(x, y, cx, cy, rx, ry):
    dx = (x + 0.5 - cx) / rx
    dy = (y + 0.5 - cy) / ry
    return dx * dx + dy * dy


def stamp(im, x, y, rows, key, flip=False):
    """Draw an ASCII pixel map. '.' and ' ' are transparent."""
    w = max(len(r) for r in rows)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch in ". ":
                continue
            c = key.get(ch)
            if c is None:
                continue
            xx = x + (w - 1 - i if flip else i)
            px(im, xx, y + j, c)


def outline(im, color=OUTLINE, diag=False, only_below=False):
    """Add a 1 px outline around the opaque silhouette."""
    src = im.copy()
    W, H = im.size
    sp = src.load()
    dp = im.load()
    nb = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    if diag:
        nb += [(1, 1), (-1, 1), (1, -1), (-1, -1)]
    for y in range(H):
        for x in range(W):
            if sp[x, y][3] != 0:
                continue
            for dx, dy in nb:
                xx, yy = x + dx, y + dy
                if 0 <= xx < W and 0 <= yy < H and sp[xx, yy][3] >= 200:
                    dp[x, y] = color
                    break
    return im


def shade_edges(im, dark, light=None, mask_color=None):
    """Darken bottom/right edge pixels, lighten top/left ones (light from top-left)."""
    src = im.copy()
    sp = src.load()
    dp = im.load()
    W, H = im.size
    for y in range(H):
        for x in range(W):
            if sp[x, y][3] == 0 or (mask_color and sp[x, y] != mask_color):
                continue
            def empty(a, b):
                return not (0 <= a < W and 0 <= b < H) or sp[a, b][3] == 0
            if dark and (empty(x + 1, y) or empty(x, y + 1)):
                dp[x, y] = dark
            elif light and (empty(x - 1, y) or empty(x, y - 1)):
                dp[x, y] = light


def recolor(im, mapping):
    out = im.copy()
    p = out.load()
    for y in range(out.height):
        for x in range(out.width):
            c = p[x, y]
            if c in mapping:
                p[x, y] = mapping[c]
    return out


def mirror(im):
    return im.transpose(Image.FLIP_LEFT_RIGHT)


def paste(dst, src, x, y):
    dst.alpha_composite(src, (x, y)) if x >= 0 and y >= 0 else _paste_clip(dst, src, x, y)


def _paste_clip(dst, src, x, y):
    sx, sy = max(0, -x), max(0, -y)
    src = src.crop((sx, sy, src.width, src.height))
    dst.alpha_composite(src, (max(0, x), max(0, y)))


def rng(seed):
    return random.Random(seed)


def dither_rect(im, x, y, w, h, c, phase=0):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if (xx + yy + phase) % 2 == 0:
                px(im, xx, yy, c)


# ------------------------------------------------------------ sheets --

MANIFEST = {}


class Sheet:
    """A grid of equally sized frames written to one PNG.

    Frames are appended left to right; `cols` fixes the grid width.
    """

    def __init__(self, name, fw, fh, cols=None, anchor=(0, 0), file=None, kind="sprite"):
        self.name, self.fw, self.fh = name, fw, fh
        self.cols = cols
        self.anchor = list(anchor)
        self.file = file or (name + ".png")
        self.frames = []
        self.anims = {}
        self.names = {}
        self.extra = {"kind": kind}

    def add(self, im, name=None):
        assert im.size == (self.fw, self.fh), (self.name, im.size)
        self.frames.append(im)
        idx = len(self.frames) - 1
        if name:
            self.names[name] = idx
        return idx

    def anim(self, name, frames, fps=6, loop=True):
        self.anims[name] = {"frames": list(frames), "fps": fps, "loop": loop}

    def save(self):
        n = len(self.frames)
        cols = self.cols or n
        rows = (n + cols - 1) // cols
        sheet = new(cols * self.fw, rows * self.fh)
        for i, f in enumerate(self.frames):
            sheet.alpha_composite(f, ((i % cols) * self.fw, (i // cols) * self.fh))
        OUT.mkdir(parents=True, exist_ok=True)
        sheet.save(OUT / self.file, optimize=True)
        entry = {
            "file": self.file,
            "frameWidth": self.fw,
            "frameHeight": self.fh,
            "columns": cols,
            "frameCount": n,
            "anchor": self.anchor,
        }
        if self.names:
            entry["frameNames"] = self.names
        if self.anims:
            entry["animations"] = self.anims
        entry.update(self.extra)
        MANIFEST[self.name] = entry
        return sheet


def single(name, im, anchor=(0, 0), **extra):
    s = Sheet(name, im.width, im.height, anchor=anchor)
    s.add(im)
    s.extra.update(extra)
    s.save()
    return s
