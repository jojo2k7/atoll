# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Furniture, wall pieces, plants, the cat. Light comes from the top left."""

import math

from .core import (C, T, OUTLINE, WHITE, hexc, alpha, new, px, rect, hline, vline,
                   frame_rect, line, ellipse, stamp, outline, mirror, rng, dither_rect,
                   Sheet, single, in_ellipse)

INK = C["ink"]
ST = C["steel"]


def box(im, x, y, w, top, front, ramp, lip=True):
    """3/4 box: `top` rows of lit top surface, then `front` rows of face."""
    r = ramp
    rect(im, x, y, w, top, r[-2])
    hline(im, x, y, w, r[-1])
    vline(im, x, y, top, r[-1])
    if front:
        rect(im, x, y + top, w, front, r[-4] if len(r) >= 4 else r[1])
        if lip:
            hline(im, x, y + top, w, r[-3])
        vline(im, x + w - 1, y + top, front, r[0])


def grain(im, x, y, w, h, c, seed, density=0.08):
    R = rng(seed)
    for j in range(h):
        if R.random() < 0.5:
            x0 = x + R.randrange(max(1, w - 4))
            L = R.randrange(3, 9)
            for i in range(L):
                if x0 + i < x + w and R.random() > density:
                    px(im, x0 + i, y + j, c)


def ol(im):
    return outline(im, OUTLINE)


# ------------------------------------------------------------------ desks --

def desk(ramp, seed):
    w, h = 40, 26
    im = new(w, h)
    r = C[ramp]
    # legs + modesty panel
    rect(im, 1, 17, 3, 8, r[1])
    rect(im, 36, 17, 3, 8, r[0])
    px(im, 1, 17, r[2])
    rect(im, 4, 17, 32, 5, r[0])
    hline(im, 4, 17, 32, INK[2])
    # top
    rect(im, 0, 0, w, 16, r[3])
    grain(im, 1, 1, w - 2, 14, r[2], seed)
    hline(im, 0, 0, w, r[4])
    vline(im, 0, 0, 16, r[4])
    rect(im, 0, 15, w, 2, r[2])
    hline(im, 0, 15, w, r[4])
    hline(im, 0, 16, w, r[1])
    vline(im, w - 1, 1, 16, r[2])
    # cable grommet
    rect(im, 32, 3, 3, 2, r[0])
    px(im, 33, 3, INK[1])
    ol(im)
    return im


def lead_desk():
    w, h = 72, 32
    im = new(w, h)
    r = C["walnut"]
    # L-shaped executive desk with drawers on both sides
    rect(im, 0, 0, w, 18, r[3])
    grain(im, 1, 1, w - 2, 16, r[2], 77)
    hline(im, 0, 0, w, r[4])
    vline(im, 0, 0, 18, r[4])
    # leather inlay
    rect(im, 18, 3, 36, 10, C["green"][1])
    frame_rect(im, 18, 3, 36, 10, C["green"][2])
    hline(im, 19, 4, 34, C["green"][3])
    hline(im, 0, 17, w, r[4])
    rect(im, 0, 18, w, 2, r[2])
    # drawers
    for x0 in (1, 53):
        rect(im, x0, 20, 18, 11, r[1])
        for k in range(3):
            hline(im, x0, 20 + k * 4, 18, r[0])
            rect(im, x0 + 7, 22 + k * 4 - 1 + 1, 4, 1, C["yellow"][2])
        vline(im, x0, 20, 11, r[2])
    rect(im, 19, 20, 34, 6, r[0])
    hline(im, 19, 20, 34, INK[2])
    # brass trim
    hline(im, 0, 19, w, C["yellow"][1])
    ol(im)
    return im


def chair(color, facing):
    """Office chair. facing 'up' = seen from behind (worker faces away)."""
    c = C[color]
    m = ST
    if facing == "up":
        # low backrest: covers the lower back only, shoulders and head stay visible
        w, h = 16, 14
        im = new(w, h)
        hline(im, 3, 11, 10, m[2])
        px(im, 2, 12, INK[1])
        px(im, 13, 12, INK[1])
        px(im, 7, 12, INK[1])
        vline(im, 7, 8, 3, m[3])
        vline(im, 8, 8, 3, m[2])
        rect(im, 1, 5, 14, 3, c[1])
        hline(im, 1, 7, 14, c[0])
        rect(im, 2, 0, 12, 6, c[2])
        hline(im, 2, 0, 12, c[3])
        vline(im, 2, 0, 6, c[3])
        vline(im, 13, 1, 5, c[1])
        rect(im, 5, 2, 6, 2, c[1])
        hline(im, 2, 5, 12, c[0])
        ol(im)
        return im
    w, h = 16, 22
    im = new(w, h)
    hline(im, 3, 19, 10, m[2])
    px(im, 2, 20, INK[1])
    px(im, 13, 20, INK[1])
    px(im, 7, 20, INK[1])
    vline(im, 7, 15, 4, m[3])
    vline(im, 8, 15, 4, m[2])
    # front: backrest behind the sitter, seat cushion in front
    rect(im, 2, 0, 12, 12, c[1])
    hline(im, 2, 0, 12, c[2])
    rect(im, 4, 2, 8, 8, c[2])
    rect(im, 1, 11, 14, 4, c[2])
    hline(im, 1, 11, 14, c[3])
    hline(im, 1, 14, 14, c[0])
    rect(im, 0, 8, 2, 5, m[3])
    rect(im, 14, 8, 2, 5, m[2])
    ol(im)
    return im


def lead_chair():
    w, h = 22, 26
    im = new(w, h)
    c = [hexc("#2a1a1c"), hexc("#4a2a2a"), hexc("#6e3a32"), hexc("#94523e")]
    hline(im, 4, 23, 14, ST[2])
    for x in (3, 10, 18):
        px(im, x, 24, INK[1])
    rect(im, 10, 18, 2, 5, ST[3])
    rect(im, 2, 0, 18, 15, c[1])
    rect(im, 2, 0, 18, 1, c[2])
    for x in range(4, 19, 4):
        vline(im, x, 2, 11, c[0])
    for x in range(5, 19, 4):
        vline(im, x, 2, 11, c[2])
    rect(im, 1, 14, 20, 5, c[2])
    hline(im, 1, 14, 20, c[3])
    hline(im, 1, 18, 20, c[0])
    rect(im, 0, 9, 3, 7, c[1])
    rect(im, 19, 9, 3, 7, c[0])
    for x in range(3, 19, 3):
        px(im, x, 1, C["yellow"][2])
    ol(im)
    return im


# --------------------------------------------------------------- monitors --

SCREEN_KINDS = ["off", "code_a", "code_b", "terminal_a", "terminal_b", "doc", "web",
                "search", "plan", "chat", "error", "question", "diff", "brief"]


def screen_content(kind, w, h, seed=0, big=False):
    """Content for a w x h screen (tiny, iconic)."""
    im = new(w, h)
    sc = C["screen"]
    cc = C["code"]
    R = rng(seed + hash(kind) % 997)
    if kind == "off":
        rect(im, 0, 0, w, h, sc[0])
        line(im, 1, h - 3, 4, 0, alpha(ST[3], 160))
        line(im, 2, h - 2, 6, 0, alpha(ST[2], 120))
        return im
    bg = sc[1]
    if kind in ("doc", "web", "plan"):
        bg = hexc("#e9e4d8")
    if kind == "error":
        bg = hexc("#3a1420")
    if kind == "question":
        bg = hexc("#2a2238")
    rect(im, 0, 0, w, h, bg)
    if kind.startswith("code") or kind == "diff":
        hline(im, 0, 0, w, sc[2])
        px(im, 1, 0, cc[3])
        px(im, 3, 0, cc[1])
        y = 2
        indent = [0, 2, 2, 4, 4, 2, 0, 2]
        k = 0 if kind == "code_a" else 3
        while y < h - 1:
            ind = indent[k % len(indent)]
            L = R.randrange(3, max(4, w - ind - 2))
            x = 1 + ind
            if kind == "diff":
                c = cc[0] if k % 3 == 1 else (cc[3] if k % 3 == 2 else cc[5])
                rect(im, 0, y, w, 1, alpha(c, 50))
                hline(im, x, y, L, c)
            else:
                while x < 1 + ind + L and x < w - 1:
                    seg = R.randrange(1, 4)
                    c = cc[R.randrange(0, 5)] if R.random() < 0.7 else cc[5]
                    hline(im, x, y, min(seg, w - 1 - x), c)
                    x += seg + 1
            y += 2
            k += 1
        if kind == "code_b":
            rect(im, min(w - 2, 3 + R.randrange(3)), y - 2 if y - 2 < h else h - 2, 1, 1, cc[5])
    elif kind.startswith("terminal"):
        rect(im, 0, 0, w, h, hexc("#0c1210"))
        g = cc[0]
        n = (h - 1) // 2
        lines = [R.randrange(2, w - 2) for _ in range(n)]
        for i, L in enumerate(lines):
            y = 1 + i * 2
            if i == n - 1:
                px(im, 1, y, g)
                if kind == "terminal_a":
                    rect(im, 3, y, 1, 1, cc[5])
                break
            hline(im, 1, y, 1, cc[1])
            hline(im, 3, y, min(L, w - 4), alpha(g, 190) if i % 2 else alpha(cc[5], 170))
    elif kind == "doc":
        rect(im, 0, 0, w, 1, hexc("#c8c0b0"))
        hline(im, 2, 2, w - 6, hexc("#3a3040"))
        y = 4
        while y < h - 1:
            hline(im, 2, y, R.randrange(w // 2, w - 3), hexc("#8a8090"))
            y += 2
    elif kind == "web":
        rect(im, 0, 0, w, 2, hexc("#c8d4e0"))
        px(im, 1, 0, cc[3])
        px(im, 2, 0, cc[1])
        px(im, 3, 0, cc[0])
        rect(im, 2, 3, w // 2 - 2, h - 5, C["blue"][4])
        rect(im, 3, 4, 3, 2, C["yellow"][3])
        for i in range(3):
            hline(im, w // 2 + 1, 4 + i * 2, w // 2 - 3, hexc("#8a8090"))
    elif kind == "search":
        for i in range(0, h - 2, 2):
            hline(im, 6, 1 + i, R.randrange(3, w - 7), cc[5] if i else cc[1])
        ellipse(im, 3, 3.5, 2.2, 2.2, cc[2])
        ellipse(im, 3, 3.5, 1.2, 1.2, bg)
        line(im, 4, 5, 5, 6, cc[2])
    elif kind == "plan":
        cols = 3
        cw = (w - 2) // cols
        for ci in range(cols):
            x = 1 + ci * cw
            hline(im, x, 1, cw - 1, hexc("#8a8090"))
            for k in range(1 + (2 - ci) % 3 + ci % 2):
                col = [C["yellow"][3], C["pink"][3], C["teal"][4], C["sky"][2]][(ci + k) % 4]
                rect(im, x, 3 + k * 3, cw - 1, 2, col)
    elif kind == "chat":
        for i in range(3):
            y = 1 + i * 3
            if y + 2 > h:
                break
            if i % 2 == 0:
                rect(im, 1, y, w // 2 + 1, 2, C["sky"][1])
            else:
                rect(im, w // 2 - 1, y, w // 2, 2, C["teal"][3])
    elif kind == "error":
        cx, cy = w // 2, h // 2
        for i in range(-2, 3):
            px(im, cx + i, cy + i, C["red"][4])
            px(im, cx + i, cy - i, C["red"][4])
            px(im, cx + i + 1, cy + i, C["red"][3])
            px(im, cx + i + 1, cy - i, C["red"][3])
        hline(im, 1, 1, w - 2, C["red"][2])
    elif kind == "question":
        q = [".###.", "#...#", "...#.", "..#..", ".....", "..#.."]
        stamp(im, w // 2 - 2, max(0, h // 2 - 3), q, {"#": C["yellow"][3]})
        hline(im, 1, h - 1, w - 2, C["purple"][3])
    elif kind == "brief":
        rect(im, 1, 1, 4, 4, C["orange"][3])
        px(im, 2, 2, WHITE)
        for i in range(3):
            hline(im, 6, 1 + i * 2, w - 8, cc[5])
        hline(im, 1, 7, w - 3, cc[0])
    return im


def monitor_front(kind, seed=0):
    w, h = 20, 18
    im = new(w, h)
    # stand
    rect(im, 8, 12, 4, 4, ST[3])
    vline(im, 8, 12, 4, ST[4])
    rect(im, 5, 15, 10, 2, ST[2])
    hline(im, 5, 15, 10, ST[4])
    # bezel
    rect(im, 0, 0, w, 13, INK[2])
    hline(im, 0, 0, w, ST[3])
    hline(im, 0, 12, w, INK[1])
    px(im, w - 3, 11, C["teal"][4] if kind != "off" else ST[2])
    im.alpha_composite(screen_content(kind, 16, 10, seed), (2, 1))
    # glass sheen
    if kind != "off":
        px(im, 2, 1, alpha(WHITE, 90))
        px(im, 3, 1, alpha(WHITE, 60))
    ol(im)
    return im


MON_SCREEN = (2, 1, 16, 10)  # x, y, w, h inside the monitor_front frame


def monitor_back(on=True):
    w, h = 20, 18
    im = new(w, h)
    rect(im, 5, 15, 10, 2, ST[2])
    hline(im, 5, 15, 10, ST[4])
    rect(im, 8, 11, 4, 4, ST[3])
    rect(im, 0, 0, w, 13, ST[3])
    hline(im, 0, 0, w, ST[5])
    vline(im, 0, 0, 13, ST[4])
    hline(im, 0, 12, w, ST[2])
    rect(im, 6, 3, 8, 6, ST[4])
    frame_rect(im, 6, 3, 8, 6, ST[2])
    # vent slits + a sticker
    for x in (7, 9, 11):
        vline(im, x, 4, 4, ST[2])
    rect(im, 2, 2, 2, 2, C["orange"][3])
    # cable down to the desk
    vline(im, 13, 13, 4, INK[1])
    ol(im)
    if on:
        # light spill around the edge
        for x in range(1, w - 1):
            if im.getpixel((x, 0))[3]:
                pass
    return im


def big_bezel():
    """Zoomed monitor: 208x140, screen rect (14, 12, 180, 100)."""
    w, h = 208, 140
    im = new(w, h)
    sx, sy, sw, sh = 14, 12, 180, 100
    # stand + foot
    rect(im, 88, 118, 32, 14, ST[3])
    vline(im, 88, 118, 14, ST[5])
    vline(im, 119, 118, 14, ST[2])
    rect(im, 64, 130, 80, 8, ST[3])
    hline(im, 64, 130, 80, ST[5])
    hline(im, 64, 137, 80, ST[1])
    rect(im, 98, 122, 12, 2, ST[2])
    # case
    rect(im, 0, 0, w, 122, INK[2])
    rect(im, 2, 2, w - 4, 118, INK[3])
    rect(im, 4, 4, w - 8, 114, INK[2])
    hline(im, 2, 2, w - 4, C["ink"][3])
    hline(im, 4, 3, w - 8, hexc("#6a5a72"))
    vline(im, 3, 4, 112, hexc("#5e4f66"))
    hline(im, 2, 119, w - 4, INK[1])
    # chin with logo + LED + buttons
    rect(im, 96, 113, 16, 3, INK[1])
    for i, c in enumerate((C["orange"][3], C["orange"][2])):
        px(im, 101 + i * 3, 114, c)
        px(im, 102 + i * 3, 114, c)
    px(im, 186, 114, C["teal"][4])
    px(im, 186, 115, C["teal"][2])
    for x in (176, 180):
        rect(im, x, 114, 2, 2, INK[1])
    # screen well
    rect(im, sx - 2, sy - 2, sw + 4, sh + 4, INK[0])
    rect(im, sx, sy, sw, sh, C["screen"][0])
    ol(im)
    glass = new(sw, sh)
    # glass overlay: diagonal sheen + faint scanlines
    for y in range(sh):
        if y % 2 == 1:
            hline(glass, 0, y, sw, (0, 0, 0, 18))
    for i in range(sh):
        x0 = 120 - i // 2
        for k in range(10):
            if 0 <= x0 + k < sw:
                px(glass, x0 + k, i, (255, 255, 255, 10))
        if 0 <= x0 + 14 < sw:
            px(glass, x0 + 14, i, (255, 255, 255, 8))
    hline(glass, 0, 0, sw, (0, 0, 0, 60))
    vline(glass, 0, 0, sh, (0, 0, 0, 40))
    return im, glass, (sx, sy, sw, sh)


# --------------------------------------------------------------- clutter --

CLUTTER = ["mug_red", "mug_white", "mug_steam", "papers", "papers_messy", "book_open",
           "books", "sticky", "figurine_robot", "figurine_dino", "headphones", "cactus",
           "succulent", "duck", "photo", "can", "pencil_cup", "notebook", "phone",
           "cube", "snack", "keyboard", "keyboard_rgb", "mouse", "nameplate", "lamp_off",
           "lamp_on", "plant_pot", "glasses_case", "coffee_cup"]


def clutter(name):
    im = new(12, 12)
    W = WHITE
    if name.startswith("mug") or name == "coffee_cup":
        col = {"mug_red": C["red"][3], "mug_white": C["cream"][4], "mug_steam": C["teal"][3],
               "coffee_cup": C["cream"][4]}[name]
        rect(im, 4, 6, 4, 4, col)
        hline(im, 4, 6, 4, C["wood"][1])
        px(im, 8, 7, col)
        px(im, 8, 8, col)
        vline(im, 4, 7, 3, alpha(WHITE, 120) if name != "mug_white" else C["cream"][3])
        if name == "coffee_cup":
            hline(im, 4, 8, 4, C["orange"][2])
        if name == "mug_steam":
            px(im, 5, 3, alpha(W, 140))
            px(im, 6, 2, alpha(W, 110))
            px(im, 5, 1, alpha(W, 80))
    elif name == "papers":
        rect(im, 2, 5, 8, 6, C["cream"][4])
        rect(im, 3, 4, 8, 6, W)
        for y in (6, 8):
            hline(im, 4, y, 5, ST[5])
    elif name == "papers_messy":
        rect(im, 1, 6, 7, 5, C["cream"][3])
        rect(im, 4, 3, 7, 6, W)
        hline(im, 5, 5, 4, ST[5])
        hline(im, 5, 7, 3, ST[5])
        rect(im, 2, 8, 3, 2, C["yellow"][3])
    elif name == "book_open":
        rect(im, 1, 5, 10, 6, C["blue"][2])
        rect(im, 1, 5, 5, 5, W)
        rect(im, 6, 5, 5, 5, C["cream"][4])
        vline(im, 6, 5, 6, C["cream"][1])
        for y in (6, 8):
            hline(im, 2, y, 3, ST[5])
            hline(im, 7, y, 3, ST[5])
    elif name == "books":
        rect(im, 1, 8, 10, 3, C["red"][2])
        hline(im, 1, 8, 10, C["red"][3])
        rect(im, 2, 5, 9, 3, C["teal"][2])
        hline(im, 2, 5, 9, C["teal"][3])
        rect(im, 1, 3, 8, 2, C["yellow"][2])
        hline(im, 1, 3, 8, C["yellow"][3])
    elif name == "sticky":
        for (x, y, c) in ((1, 4, C["yellow"][3]), (5, 2, C["pink"][3]), (6, 6, C["teal"][4])):
            rect(im, x, y, 4, 4, c)
            hline(im, x + 1, y + 1, 2, alpha(INK[2], 140))
    elif name == "figurine_robot":
        rect(im, 4, 2, 4, 3, ST[5])
        px(im, 5, 3, C["teal"][4])
        px(im, 6, 3, C["teal"][4])
        vline(im, 6, 0, 2, ST[4])
        px(im, 6, 0, C["red"][3])
        rect(im, 3, 5, 6, 4, ST[4])
        px(im, 5, 6, C["orange"][3])
        rect(im, 4, 9, 1, 2, ST[3])
        rect(im, 7, 9, 1, 2, ST[3])
    elif name == "figurine_dino":
        stamp(im, 2, 3, ["....GG.", "....GGG", "...GGg.", ".GGGGg.", "GGGGGg.", "..G..G."],
              {"G": C["green"][4], "g": C["green"][3]})
        px(im, 7, 3, INK[1])
    elif name == "headphones":
        rect(im, 2, 6, 3, 4, INK[2])
        rect(im, 7, 6, 3, 4, INK[2])
        px(im, 3, 7, C["pink"][3])
        px(im, 8, 7, C["pink"][3])
        hline(im, 3, 3, 6, INK[2])
        px(im, 2, 4, INK[2])
        px(im, 9, 4, INK[2])
        px(im, 2, 5, INK[2])
        px(im, 9, 5, INK[2])
    elif name == "cactus":
        rect(im, 4, 8, 4, 3, C["orange"][1])
        hline(im, 4, 8, 4, C["orange"][2])
        rect(im, 5, 2, 2, 6, C["green"][3])
        vline(im, 5, 2, 6, C["green"][4])
        rect(im, 3, 4, 1, 2, C["green"][3])
        px(im, 4, 5, C["green"][3])
        rect(im, 8, 3, 1, 2, C["green"][3])
        px(im, 7, 4, C["green"][3])
        px(im, 6, 1, C["pink"][3])
    elif name == "succulent":
        rect(im, 3, 8, 6, 3, C["cream"][3])
        hline(im, 3, 8, 6, C["cream"][4])
        for (x, y) in ((4, 6), (6, 5), (7, 6), (5, 7), (3, 7), (8, 7)):
            px(im, x, y, C["teal"][4])
        px(im, 5, 6, C["teal"][3])
        px(im, 6, 6, C["green"][4])
    elif name == "duck":
        rect(im, 3, 6, 6, 4, C["yellow"][3])
        rect(im, 6, 3, 3, 3, C["yellow"][3])
        px(im, 9, 5, C["orange"][3])
        px(im, 7, 4, INK[1])
        hline(im, 3, 9, 6, C["yellow"][2])
    elif name == "photo":
        rect(im, 2, 3, 8, 7, C["oak"][2])
        rect(im, 3, 4, 6, 4, C["sky"][2])
        rect(im, 3, 7, 6, 1, C["green"][3])
        px(im, 5, 5, C["skin0"][2])
        px(im, 7, 5, C["skin2"][2])
        hline(im, 2, 10, 8, C["oak"][1])
    elif name == "can":
        rect(im, 4, 4, 4, 7, C["purple"][3])
        hline(im, 4, 4, 4, ST[5])
        vline(im, 4, 5, 6, C["purple"][4])
        px(im, 5, 7, C["yellow"][3])
        px(im, 6, 7, C["yellow"][3])
    elif name == "pencil_cup":
        rect(im, 3, 6, 5, 5, ST[4])
        vline(im, 3, 6, 5, ST[5])
        for (x, c) in ((4, C["yellow"][3]), (5, C["red"][3]), (6, C["blue"][4])):
            vline(im, x, 2 + (x % 2), 4, c)
    elif name == "notebook":
        rect(im, 2, 4, 8, 7, C["teal"][2])
        vline(im, 2, 4, 7, C["teal"][1])
        rect(im, 4, 5, 5, 2, C["cream"][4])
        vline(im, 8, 7, 4, C["red"][3])
    elif name == "phone":
        rect(im, 4, 4, 4, 7, INK[2])
        rect(im, 5, 5, 2, 4, C["blue"][3])
        px(im, 5, 5, C["sky"][3])
    elif name == "cube":
        cols = [C["red"][3], C["yellow"][3], C["teal"][4], C["blue"][4], W, C["orange"][3]]
        for j in range(3):
            for i in range(3):
                rect(im, 3 + i * 2, 4 + j * 2, 2, 2, cols[(i + j * 2) % 6])
    elif name == "snack":
        rect(im, 2, 6, 8, 4, C["pink"][3])
        hline(im, 2, 6, 8, C["pink"][4])
        for x in (3, 5, 7):
            px(im, x, 8, C["orange"][2])
        ellipse(im, 6, 5, 2, 1.4, C["oak"][3])
        px(im, 6, 4, C["pink"][4])
    elif name in ("keyboard", "keyboard_rgb"):
        rect(im, 0, 7, 12, 4, ST[2] if name == "keyboard" else INK[2])
        hline(im, 0, 7, 12, ST[4] if name == "keyboard" else INK[3])
        for j in range(2):
            for i in range(0, 11, 2):
                c = ST[5]
                if name == "keyboard_rgb":
                    c = [C["pink"][3], C["purple"][4], C["blue"][4], C["teal"][4], C["yellow"][3], C["orange"][3]][i // 2]
                px(im, 1 + i, 8 + j, c)
    elif name == "mouse":
        rect(im, 5, 7, 3, 4, ST[5])
        px(im, 6, 7, ST[3])
        px(im, 5, 10, ST[4])
    elif name == "nameplate":
        rect(im, 1, 7, 10, 3, C["walnut"][2])
        hline(im, 1, 7, 10, C["yellow"][3])
        hline(im, 3, 8, 6, C["yellow"][2])
    elif name in ("lamp_off", "lamp_on"):
        rect(im, 3, 10, 5, 1, ST[2])
        vline(im, 5, 5, 5, ST[3])
        line(im, 5, 5, 8, 3, ST[3])
        shade = C["teal"][3]
        rect(im, 7, 2, 4, 3, shade)
        hline(im, 7, 2, 4, C["teal"][4])
        if name == "lamp_on":
            hline(im, 7, 5, 4, C["yellow"][4])
    elif name == "plant_pot":
        rect(im, 3, 7, 6, 4, C["orange"][2])
        hline(im, 3, 7, 6, C["orange"][3])
        for (x, y) in ((4, 5), (5, 3), (6, 4), (7, 2), (8, 5), (3, 4), (6, 6), (5, 6)):
            px(im, x, y, C["green"][4])
        px(im, 6, 3, C["green"][5])
        px(im, 4, 6, C["green"][3])
    elif name == "glasses_case":
        rect(im, 2, 7, 8, 3, C["red"][1])
        hline(im, 2, 7, 8, C["red"][2])
    ol(im)
    return im


def clutter_sheet():
    s = Sheet("clutter", 12, 12, cols=10, anchor=(6, 11))
    for n in CLUTTER:
        s.add(clutter(n), n)
    s.save()
    return s


# ----------------------------------------------------------------- server --

def server_rack(phase):
    w, h = 28, 60
    im = new(w, h)
    body = ST
    rect(im, 0, 0, w, h - 2, INK[2])
    hline(im, 0, 0, w, body[3])
    rect(im, 0, 1, w, 3, body[2])
    hline(im, 0, 1, w, body[4])
    vline(im, 0, 0, h - 2, body[3])
    vline(im, w - 1, 0, h - 2, INK[1])
    # feet
    rect(im, 1, h - 2, 3, 2, INK[1])
    rect(im, w - 4, h - 2, 3, 2, INK[1])
    R = rng(11)
    leds = [C["teal"][4], C["code"][0], C["yellow"][3], C["sky"][3], C["red"][4]]
    y = 6
    unit = 0
    while y < h - 6:
        uh = [4, 4, 6, 4, 8, 4][unit % 6]
        rect(im, 2, y, w - 4, uh - 1, body[1])
        hline(im, 2, y, w - 4, body[3])
        if uh == 8:
            # disk shelf
            for i in range(6):
                rect(im, 3 + i * 4, y + 2, 3, uh - 4, body[2])
                pr = (unit * 7 + i * 3 + phase * 5) % 9
                px(im, 4 + i * 4, y + uh - 3, leds[1] if pr < 6 else body[0])
        else:
            for i in range(3):
                seed = unit * 13 + i * 5
                on = ((seed + phase * (3 + i)) % 7) < 4 + (i == 0)
                col = leds[(unit + i) % 4] if on else body[0]
                px(im, 4 + i * 2, y + 1, col)
            for k in range(4):
                px(im, 12 + k * 3, y + 1, body[0])
                px(im, 13 + k * 3, y + 1, body[3] if (k + phase + unit) % 3 else C["code"][0])
            if uh == 6:
                hline(im, 4, y + 3, 18, body[2])
        y += uh
        unit += 1
    # glass door sheen
    line(im, 4, h - 6, 14, 6, alpha(WHITE, 26))
    line(im, 5, h - 6, 15, 6, alpha(WHITE, 18))
    ol(im)
    return im


def net_cabinet(phase):
    w, h = 20, 30
    im = new(w, h)
    rect(im, 0, 0, w, h, ST[2])
    rect(im, 0, 0, w, 4, ST[4])
    hline(im, 0, 0, w, ST[5])
    vline(im, w - 1, 4, h - 4, ST[1])
    # switch + patch panel
    rect(im, 2, 6, 16, 6, INK[2])
    for i in range(7):
        px(im, 3 + i * 2, 8, ST[4])
        on = (i * 3 + phase) % 4 != 0
        px(im, 3 + i * 2, 10, C["code"][0] if on else INK[1])
    # UPS
    rect(im, 2, 15, 16, 12, INK[2])
    rect(im, 4, 17, 6, 3, C["blue"][1])
    hline(im, 5, 18, 2 + phase % 3, C["teal"][4])
    px(im, 14, 18, C["code"][0] if phase % 2 == 0 else C["teal"][2])
    for k in range(3):
        hline(im, 4, 22 + k * 2, 12, ST[1])
    ol(im)
    return im


# --------------------------------------------------------------- kitchen --

def counter():
    w, h = 72, 30
    im = new(w, h)
    top = C["cream"]
    # counter top (light stone)
    rect(im, 0, 0, w, 8, top[3])
    hline(im, 0, 0, w, top[4])
    dither_rect(im, 1, 2, w - 2, 5, alpha(top[2], 90), 1)
    hline(im, 0, 7, w, top[1])
    # sink
    rect(im, 34, 2, 16, 5, ST[4])
    rect(im, 35, 3, 14, 3, ST[3])
    hline(im, 35, 3, 14, ST[2])
    vline(im, 42, 0, 3, ST[5])
    px(im, 43, 0, ST[5])
    px(im, 44, 1, ST[5])
    # cabinets
    cab = C["sage"]
    rect(im, 0, 8, w, 20, cab[2])
    for i in range(4):
        x0 = 1 + i * 18
        rect(im, x0, 10, 16, 16, cab[2])
        frame_rect(im, x0, 10, 16, 16, cab[1])
        hline(im, x0 + 1, 11, 14, cab[3])
        rect(im, x0 + (12 if i % 2 == 0 else 2), 13, 2, 1, C["yellow"][3])
    rect(im, 0, 28, w, 2, INK[2])
    ol(im)
    return im


def upper_cabinets():
    w, h = 72, 22
    im = new(w, h)
    cab = C["sage"]
    rect(im, 0, 0, w, 18, cab[2])
    hline(im, 0, 0, w, cab[4])
    for i in range(3):
        x0 = 1 + i * 24
        if i == 1:
            # open shelf with jars and mugs
            rect(im, x0, 1, 22, 16, cab[0])
            hline(im, x0, 8, 22, cab[3])
            for k, c in enumerate((C["red"][3], C["cream"][4], C["teal"][3], C["yellow"][3])):
                rect(im, x0 + 2 + k * 5, 4, 3, 4, c)
                px(im, x0 + 2 + k * 5, 4, alpha(WHITE, 110))
            for k, c in enumerate((C["cream"][3], C["orange"][2], C["cream"][3])):
                rect(im, x0 + 3 + k * 6, 11, 4, 6, c)
                hline(im, x0 + 3 + k * 6, 11, 4, C["walnut"][2])
        else:
            frame_rect(im, x0, 1, 22, 16, cab[1])
            hline(im, x0 + 1, 2, 20, cab[3])
            rect(im, x0 + (19 if i == 0 else 2), 13, 1, 3, C["yellow"][3])
    rect(im, 0, 18, w, 2, cab[1])
    # under-cabinet light strip
    hline(im, 2, 20, w - 4, C["yellow"][4])
    ol(im)
    return im


def coffee_machine(frame):
    w, h = 18, 22
    im = new(w, h)
    b = INK
    rect(im, 1, 0, 16, 20, C["red"][2])
    hline(im, 1, 0, 16, C["red"][4])
    vline(im, 1, 0, 20, C["red"][3])
    vline(im, 16, 1, 19, C["red"][1])
    # bean hopper
    rect(im, 3, 1, 6, 4, hexc("#3a2418"))
    hline(im, 3, 1, 6, ST[5])
    # dial + display
    rect(im, 11, 2, 4, 3, b[1])
    px(im, 12, 3, C["code"][0] if frame else C["teal"][2])
    # spout area
    rect(im, 3, 8, 12, 10, b[2])
    rect(im, 7, 8, 4, 2, ST[4])
    # drip tray
    rect(im, 3, 17, 12, 2, ST[3])
    hline(im, 3, 17, 12, ST[5])
    # cup
    rect(im, 7, 13, 4, 4, WHITE)
    px(im, 11, 14, WHITE)
    if frame:
        for k in range(frame + 1):
            if 10 + k < 13:
                px(im, 9, 10 + k, hexc("#6a3a24"))
        hline(im, 7, 13, 4, hexc("#7a4a2a") if frame > 1 else WHITE)
    rect(im, 0, 20, 18, 2, b[1])
    ol(im)
    return im


def steam(frame):
    w, h = 10, 16
    im = new(w, h)
    R = rng(5)
    for k in range(3):
        t = (frame + k * 2) % 6
        y = 14 - t * 2
        x = 4 + int(round(math.sin((t + k) * 1.3) * 2))
        a = max(0, 150 - t * 24)
        px(im, x, y, alpha(WHITE, a))
        px(im, x + 1, y, alpha(WHITE, a // 2))
        px(im, x, y - 1, alpha(WHITE, a // 2))
    return im


def fridge():
    w, h = 24, 54
    im = new(w, h)
    c = [hexc("#9aa8ac"), hexc("#c0ccce"), hexc("#dfe8e6"), hexc("#f2f6f2")]
    rect(im, 0, 0, w, h - 2, c[2])
    hline(im, 0, 0, w, c[3])
    vline(im, 0, 0, h - 2, c[3])
    vline(im, w - 1, 1, h - 3, c[0])
    rect(im, 0, 0, w, 3, c[1])
    hline(im, 0, 0, w, c[3])
    hline(im, 1, 18, w - 2, c[0])
    vline(im, 19, 6, 9, c[0])
    vline(im, 19, 22, 14, c[0])
    vline(im, 20, 6, 9, c[3])
    vline(im, 20, 22, 14, c[3])
    # magnets + a child's drawing + a note
    rect(im, 4, 22, 9, 8, WHITE)
    for (x, y) in ((6, 26), (7, 25), (8, 26), (9, 27), (10, 26)):
        px(im, x, y, C["blue"][4])
    rect(im, 5, 23, 2, 2, C["yellow"][3])
    px(im, 8, 21, C["red"][3])
    rect(im, 5, 8, 5, 5, C["yellow"][3])
    hline(im, 6, 10, 3, alpha(INK[2], 150))
    px(im, 12, 7, C["teal"][3])
    px(im, 14, 12, C["pink"][3])
    rect(im, 1, h - 2, w - 2, 2, INK[2])
    ol(im)
    return im


def water_cooler(frame):
    w, h = 14, 32
    im = new(w, h)
    # bottle
    blue = [hexc("#4a8ab8"), hexc("#6ab0dc"), hexc("#9ad2f0"), hexc("#d0eefa")]
    ellipse(im, 7, 7, 5.5, 6.5, alpha(blue[1], 220))
    rect(im, 3, 2, 8, 10, alpha(blue[1], 220))
    rect(im, 4, 3, 1, 7, alpha(blue[3], 200))
    hline(im, 3, 2, 8, blue[2])
    rect(im, 5, 12, 4, 2, blue[0])
    rect(im, 3, 6, 8, 1, alpha(blue[0], 160))
    for k in range(2):
        by = 10 - ((frame * 3 + k * 4) % 8)
        px(im, 6 + k * 2, by, blue[3])
    # body
    rect(im, 1, 14, 12, 16, ST[6])
    hline(im, 1, 14, 12, ST[7])
    vline(im, 12, 15, 15, ST[4])
    rect(im, 3, 17, 2, 2, C["red"][3])
    rect(im, 9, 17, 2, 2, C["blue"][3])
    rect(im, 3, 21, 8, 3, ST[2])
    rect(im, 1, 30, 12, 2, INK[2])
    ol(im)
    return im


# ------------------------------------------------------------------ lounge --

def sofa():
    w, h = 60, 32
    im = new(w, h)
    c = C["teal"]
    # back
    rect(im, 2, 0, 56, 14, c[2])
    hline(im, 2, 0, 56, c[3])
    for i in range(3):
        x0 = 4 + i * 18
        rect(im, x0, 2, 16, 10, c[2])
        hline(im, x0, 2, 16, c[3])
        hline(im, x0, 11, 16, c[1])
        px(im, x0 + 8, 6, c[1])
    # seat cushions
    rect(im, 2, 14, 56, 9, c[3])
    for i in range(3):
        x0 = 4 + i * 18
        rect(im, x0, 14, 17, 8, c[3])
        hline(im, x0, 14, 17, c[4])
        vline(im, x0 + 17, 14, 8, c[1])
    rect(im, 2, 22, 56, 6, c[1])
    hline(im, 2, 22, 56, c[2])
    # arms
    for x0, sh in ((0, c[3]), (52, c[2])):
        rect(im, x0, 6, 8, 22, sh)
        hline(im, x0, 6, 8, c[4])
        vline(im, x0 + 7 if x0 else x0, 7, 21, c[1])
    # throw pillow + blanket
    rect(im, 8, 9, 7, 6, C["orange"][3])
    hline(im, 8, 9, 7, C["orange"][4])
    px(im, 11, 12, C["orange"][2])
    rect(im, 42, 13, 9, 9, C["cream"][3])
    for y in range(13, 22, 2):
        hline(im, 42, y, 9, C["red"][3])
    # legs
    for x in (3, 55):
        rect(im, x, 28, 2, 3, C["walnut"][1])
    ol(im)
    return im


def armchair():
    """Facing right (the sitter looks right)."""
    w, h = 26, 30
    im = new(w, h)
    c = C["orange"]
    # backrest on the left side
    rect(im, 1, 0, 8, 26, c[1])
    hline(im, 1, 0, 8, c[3])
    vline(im, 1, 0, 26, c[2])
    rect(im, 3, 2, 5, 20, c[2])
    # seat
    rect(im, 8, 13, 16, 9, c[3])
    hline(im, 8, 13, 16, c[4])
    rect(im, 8, 22, 17, 4, c[1])
    # near arm (front)
    rect(im, 4, 18, 21, 6, c[2])
    hline(im, 4, 18, 21, c[3])
    hline(im, 4, 23, 21, c[0])
    for x in (3, 22):
        rect(im, x, 26, 2, 3, C["walnut"][1])
    ol(im)
    return im


def coffee_table():
    w, h = 36, 18
    im = new(w, h)
    r = C["oak"]
    rect(im, 0, 0, w, 10, r[3])
    hline(im, 0, 0, w, r[4])
    grain(im, 1, 1, w - 2, 8, r[2], 5)
    rect(im, 0, 10, w, 2, r[1])
    for x in (2, 32):
        rect(im, x, 12, 2, 5, r[0])
    hline(im, 4, 14, 28, r[1])
    # magazines + plant + mugs
    rect(im, 4, 2, 9, 6, C["blue"][4])
    rect(im, 5, 3, 9, 6, C["cream"][4])
    rect(im, 6, 4, 4, 2, C["red"][3])
    rect(im, 24, 3, 5, 4, C["cream"][3])
    for (x, y) in ((25, 1), (26, 0), (27, 1), (24, 2), (28, 2), (26, 2)):
        px(im, x, y, C["green"][4])
    px(im, 18, 4, WHITE)
    px(im, 19, 4, WHITE)
    px(im, 18, 5, WHITE)
    px(im, 19, 5, C["cream"][3])
    ol(im)
    return im


def floor_lamp(on):
    w, h = 14, 44
    im = new(w, h)
    rect(im, 3, 41, 8, 2, INK[2])
    vline(im, 7, 12, 29, ST[3])
    vline(im, 6, 12, 29, ST[5])
    shade = [C["cream"][2], C["cream"][3], C["cream"][4], C["yellow"][4]]
    for j in range(12):
        x0 = 3 - j // 4
        ww = 8 + (j // 4) * 2
        rect(im, x0, j, ww, 1, shade[2] if on else shade[1])
        px(im, x0, j, shade[3] if on else shade[2])
        px(im, x0 + ww - 1, j, shade[1] if on else shade[0])
    if on:
        hline(im, 1, 11, 12, C["yellow"][4])
    ol(im)
    return im


def bookshelf():
    w, h = 36, 50
    im = new(w, h)
    r = C["walnut"]
    rect(im, 0, 0, w, h, r[2])
    hline(im, 0, 0, w, r[4])
    vline(im, 0, 0, h, r[3])
    vline(im, w - 1, 0, h, r[1])
    R = rng(21)
    cols = [C["red"][2], C["red"][3], C["teal"][2], C["teal"][3], C["yellow"][2], C["blue"][3],
            C["purple"][3], C["cream"][3], C["green"][3], C["orange"][2], C["sage"][3]]
    for s in range(4):
        y0 = 3 + s * 12
        rect(im, 2, y0, w - 4, 10, r[0])
        x = 3
        while x < w - 4:
            if s == 1 and 18 < x < 26:
                # a little plant on the shelf
                rect(im, x, y0 + 6, 5, 4, C["orange"][2])
                for (a, b) in ((1, 3), (2, 1), (3, 2), (0, 4), (4, 4), (2, 3), (3, 5)):
                    px(im, x + a, y0 + b, C["green"][4])
                x += 7
                continue
            if s == 3 and x > 22:
                # leaning frame
                rect(im, x, y0 + 3, 7, 7, C["oak"][3])
                rect(im, x + 1, y0 + 4, 5, 5, C["sky"][2])
                x += 9
                continue
            bw = R.choice((2, 2, 3))
            bh = R.randrange(6, 10)
            c = R.choice(cols)
            rect(im, x, y0 + 10 - bh, bw, bh, c)
            px(im, x, y0 + 10 - bh, alpha(WHITE, 70))
            if bw == 3:
                hline(im, x, y0 + 10 - bh + 2, 3, alpha(WHITE, 60))
            x += bw
            if R.random() < 0.12:
                # a book lying flat
                rect(im, x, y0 + 8, 6, 2, R.choice(cols))
                x += 7
        hline(im, 2, y0 + 10, w - 4, r[3])
        hline(im, 2, y0 + 11, w - 4, r[1])
    ol(im)
    return im


def record_player(frame):
    w, h = 18, 22
    im = new(w, h)
    r = C["oak"]
    # side table
    rect(im, 1, 8, 16, 3, r[3])
    hline(im, 1, 8, 16, r[4])
    rect(im, 2, 11, 2, 10, r[1])
    rect(im, 14, 11, 2, 10, r[0])
    hline(im, 3, 16, 12, r[1])
    # crate of records
    rect(im, 4, 17, 10, 4, r[2])
    for x in range(5, 13, 2):
        vline(im, x, 15, 2, [C["red"][3], C["teal"][3], C["yellow"][3], C["purple"][3]][(x // 2) % 4])
    # turntable
    rect(im, 2, 2, 14, 7, INK[2])
    hline(im, 2, 2, 14, INK[3])
    ellipse(im, 8, 5.5, 4.5, 2.6, INK[1])
    px(im, 8, 5, C["red"][3] if frame % 2 == 0 else C["orange"][3])
    px(im, 6 + (frame % 4), 4 + (frame % 2), alpha(WHITE, 90))
    line(im, 14, 3, 11, 6, ST[5])
    ol(im)
    return im


# ------------------------------------------------------------------ plants --

def leaf_cluster(im, pts, ramp, seed):
    R = rng(seed)
    for (x, y, r) in pts:
        ellipse(im, x, y, r, r * 0.8, ramp[2])
        ellipse(im, x - 0.6, y - 0.6, r * 0.6, r * 0.5, ramp[3])
        px(im, int(x - r * 0.4), int(y - r * 0.4), ramp[4])


def pot(im, x, y, w, h, ramp):
    rect(im, x, y, w, h, ramp[2])
    hline(im, x, y, w, ramp[3])
    rect(im, x - 1, y, w + 2, 2, ramp[3])
    hline(im, x - 1, y, w + 2, ramp[4] if len(ramp) > 4 else ramp[3])
    vline(im, x + w - 1, y + 2, h - 2, ramp[1])
    hline(im, x, y + h - 1, w, ramp[1])


def monstera(sway):
    w, h = 28, 40
    im = new(w, h)
    g = C["green"]
    for (x0, y0, x1, y1) in ((14, 30, 6, 10), (14, 30, 21, 8), (14, 30, 13, 4), (14, 30, 4, 20), (14, 30, 24, 18)):
        line(im, x0, y0, x1 + (sway if y1 < 12 else 0), y1, g[1])
    leaves = [(6, 10, 5), (21, 8, 5), (13, 5, 5), (4, 19, 4.5), (24, 18, 4.5), (13, 15, 5), (8, 24, 4), (20, 25, 4)]
    for i, (x, y, r) in enumerate(leaves):
        dx = sway if y < 14 else 0
        ellipse(im, x + dx, y, r, r * 0.85, g[2])
        ellipse(im, x + dx - 1, y - 1, r * 0.7, r * 0.55, g[3])
        px(im, int(x + dx - r * 0.5), int(y - r * 0.4), g[4])
        # monstera splits
        line(im, int(x + dx), int(y), int(x + dx + r * 0.8), int(y - r * 0.3), g[1])
        line(im, int(x + dx), int(y + 1), int(x + dx - r * 0.7), int(y + r * 0.6), g[1])
    pot(im, 8, 30, 12, 9, C["cream"])
    hline(im, 9, 34, 10, C["sage"][3])
    ol(im)
    return im


def snake_plant():
    w, h = 16, 28
    im = new(w, h)
    g = [C["green"][0], C["green"][2], C["green"][3], C["green"][4], C["yellow"][3]]
    blades = [(4, 6, -1), (7, 1, 0), (10, 4, 1), (6, 9, -1), (12, 10, 1), (9, 8, 0)]
    for (x, top, lean) in blades:
        for y in range(top, 20):
            xx = x + (lean if y < top + 5 else 0)
            px(im, xx, y, g[2])
            px(im, xx + 1, y, g[1])
            if (y + x) % 4 == 0:
                px(im, xx, y, g[3])
        px(im, x + (lean if True else 0), top, g[4])
        px(im, x + 1, top + 1, g[4])
    pot(im, 3, 20, 10, 7, C["steel"][2:])
    ol(im)
    return im


def ficus(sway):
    w, h = 24, 46
    im = new(w, h)
    g = C["green"]
    vline(im, 11, 18, 18, C["walnut"][2])
    vline(im, 12, 22, 14, C["walnut"][1])
    line(im, 11, 26, 6, 20, C["walnut"][2])
    line(im, 12, 24, 17, 18, C["walnut"][2])
    R = rng(9)
    for k in range(46):
        a = R.random() * math.pi * 2
        rr = R.random() ** 0.6
        x = 12 + math.cos(a) * 9 * rr + (sway if k % 2 else 0) * (1 - rr)
        y = 14 + math.sin(a) * 11 * rr
        c = g[2] if (y - x * 0.3) > 8 else g[3]
        if R.random() < 0.15:
            c = g[4]
        if y > 18 and x > 12:
            c = g[1] if R.random() < 0.5 else g[2]
        ellipse(im, x, y, 1.6, 1.2, c)
    pot(im, 6, 36, 12, 9, C["orange"])
    ol(im)
    return im


def hanging_plant(sway):
    w, h = 18, 30
    im = new(w, h)
    g = C["green"]
    line(im, 9, 0, 4, 8, ST[4])
    line(im, 9, 0, 14, 8, ST[4])
    rect(im, 4, 8, 11, 5, C["cream"][3])
    hline(im, 4, 8, 11, C["cream"][4])
    hline(im, 4, 12, 11, C["cream"][1])
    for (x0, L) in ((4, 14), (7, 9), (11, 16), (14, 11)):
        for j in range(L):
            x = x0 + int(round(math.sin(j * 0.6 + sway) * 1.2))
            y = 12 + j
            px(im, x, y, g[2] if j % 3 else g[4])
            if j % 3 == 0:
                px(im, x + 1, y, g[3])
    ol(im)
    return im


def cactus_big():
    w, h = 14, 24
    im = new(w, h)
    g = C["green"]
    rect(im, 5, 3, 4, 14, g[3])
    vline(im, 5, 3, 14, g[4])
    vline(im, 8, 4, 13, g[2])
    rect(im, 1, 7, 3, 6, g[3])
    rect(im, 3, 11, 2, 2, g[3])
    rect(im, 10, 5, 3, 5, g[3])
    rect(im, 9, 9, 2, 2, g[3])
    px(im, 6, 2, C["pink"][3])
    px(im, 7, 2, C["pink"][4])
    for (x, y) in ((6, 6), (7, 10), (6, 14), (2, 9), (11, 7)):
        px(im, x, y, g[5])
    pot(im, 3, 17, 8, 6, C["orange"])
    ol(im)
    return im


# ------------------------------------------------------------- wall items --

def window_frame():
    w, h = 54, 40
    im = new(w, h)
    f = [C["cream"][1], C["cream"][3], C["cream"][4], WHITE]
    rect(im, 0, 0, w, h - 4, f[2])
    rect(im, 3, 3, w - 6, h - 10, T)
    # clear glass area (transparent)
    im2 = im.load()
    for y in range(3, h - 7):
        for x in range(3, w - 3):
            im2[x, y] = T
    vline(im, w // 2, 3, h - 10, f[2])
    hline(im, 3, 17, w - 6, f[2])
    hline(im, 0, 0, w, f[3])
    vline(im, 0, 0, h - 4, f[3])
    vline(im, w - 1, 0, h - 4, f[1])
    # sill
    rect(im, -1 + 0, h - 6, w, 4, f[3])
    hline(im, 0, h - 3, w, f[1])
    rect(im, 2, h - 2, w - 4, 2, alpha(INK[1], 90))
    # inner shadow at top of glass
    hline(im, 3, 3, w - 6, alpha(INK[2], 110))
    vline(im, 3, 3, h - 10, alpha(INK[2], 70))
    ol(im)
    return im


WINDOW_GLASS = (3, 3, 48, 30)


def window_sky(mode):
    w, h = WINDOW_GLASS[2], WINDOW_GLASS[3]
    im = new(w, h)
    if mode == "day":
        top, mid, bot = hexc("#5d97d0"), hexc("#86b8e2"), hexc("#bfe0f2")
    elif mode == "dawn":
        top, mid, bot = hexc("#6a78b8"), hexc("#d89aa8"), hexc("#f6d0a0")
    elif mode == "dusk":
        top, mid, bot = hexc("#3a3a78"), hexc("#b0567a"), hexc("#f0904a")
    else:
        top, mid, bot = hexc("#0e1430"), hexc("#18204a"), hexc("#26305e")
    for y in range(h):
        t = y / (h - 1)
        c = top if t < 0.35 else (mid if t < 0.7 else bot)
        hline(im, 0, y, w, c)
        # dithered band transitions
        if 0.3 < t < 0.4 or 0.65 < t < 0.75:
            for x in range(w):
                if (x + y) % 2 == 0:
                    px(im, x, y, top if t < 0.5 else mid)
    R = rng(3)
    if mode in ("day", "dawn"):
        for (cx, cy, s) in ((10, 8, 1.0), (34, 12, 1.3), (44, 5, 0.8)):
            cc = WHITE if mode == "day" else hexc("#fbe6d8")
            ellipse(im, cx, cy, 5 * s, 2 * s, cc)
            ellipse(im, cx + 2 * s, cy - 1.2 * s, 3 * s, 1.8 * s, cc)
            hline(im, int(cx - 4 * s), int(cy + 1.5 * s), int(8 * s), alpha(hexc("#a8c8e8"), 200))
    if mode == "dusk":
        ellipse(im, 36, h - 2, 5, 4, hexc("#ffd27a"))
    if mode == "night":
        for k in range(16):
            x, y = R.randrange(w), R.randrange(h - 8)
            px(im, x, y, WHITE if k % 3 else hexc("#a9c7ff"))
        ellipse(im, 38, 8, 3.2, 3.2, hexc("#f3efd0"))
        ellipse(im, 39.5, 7, 2.6, 2.6, top)
    # distant skyline
    sk = {"day": hexc("#7aa0c0"), "dawn": hexc("#9a8aa8"), "dusk": hexc("#5a3a68"), "night": hexc("#121830")}[mode]
    xs = 0
    hs = [8, 12, 6, 10, 14, 7, 11, 9, 5, 13, 8]
    i = 0
    while xs < w:
        bw = 3 + (i * 7) % 4
        bh = hs[i % len(hs)]
        rect(im, xs, h - bh, bw, bh, sk)
        if mode == "night":
            for yy in range(h - bh + 2, h, 3):
                if (xs + yy) % 5 < 2:
                    px(im, xs + 1, yy, hexc("#f6d36a"))
        xs += bw + 1
        i += 1
    # tree tops in front
    tree = {"day": C["green"][2], "dawn": C["green"][1], "dusk": hexc("#2a2440"), "night": hexc("#0a0e1c")}[mode]
    for (cx, r) in ((6, 5), (14, 4), (42, 6)):
        ellipse(im, cx, h + 1, r, r, tree)
    return im


def window_beam(mode, w=96, h=110):
    """Light falling through a window onto the floor, slanting right."""
    im = new(w, h)
    col = {"day": hexc("#fff3c8"), "dawn": hexc("#ffd8b0"), "dusk": hexc("#ffb070"), "night": hexc("#a8c0ff")}[mode]
    a1 = {"day": 34, "dawn": 26, "dusk": 30, "night": 14}[mode]
    for y in range(h):
        t = y / h
        x0 = int(t * (w - 52))
        width = 50 - int(t * 6)
        fade = 1.0 - max(0, t - 0.55) / 0.45
        for x in range(x0, min(w, x0 + width)):
            inner = x0 + 4 <= x < x0 + width - 4
            a = int((a1 if inner else a1 * 0.55) * fade)
            # mullion shadows
            rel = x - x0
            if 22 <= rel <= 25:
                a = int(a * 0.35)
            if a > 0:
                px(im, x, y, alpha(col, a))
    return im


def clock_face():
    w = h = 18
    im = new(w, h)
    ellipse(im, 9, 9, 8.5, 8.5, C["walnut"][2])
    ellipse(im, 9, 9, 7, 7, C["cream"][4])
    ellipse(im, 8.5, 8.5, 6.5, 6.5, WHITE)
    for k in range(12):
        a = k * math.pi / 6
        x = 9 + math.sin(a) * 5.6
        y = 9 - math.cos(a) * 5.6
        px(im, int(x), int(y), INK[2] if k % 3 == 0 else ST[4])
    px(im, 8, 8, INK[1])
    px(im, 9, 8, C["red"][3])
    ol(im)
    return im


def clock_hand(k, length, col):
    w = h = 18
    im = new(w, h)
    a = k * math.pi / 6
    line(im, 9, 9, int(round(9 + math.sin(a) * length)), int(round(9 - math.cos(a) * length)), col)
    return im


def kanban():
    w, h = 52, 34
    im = new(w, h)
    rect(im, 0, 0, w, h, C["oak"][3])
    rect(im, 2, 2, w - 4, h - 6, hexc("#f3f1ea"))
    hline(im, 0, 0, w, C["oak"][4])
    rect(im, 0, h - 4, w, 4, C["oak"][2])
    for x in (18, 34):
        vline(im, x, 3, h - 8, ST[5])
    cols = [C["yellow"][3], C["pink"][3], C["teal"][4], C["sky"][2], C["orange"][4]]
    counts = (4, 2, 3)
    for ci, n in enumerate(counts):
        hline(im, 4 + ci * 16, 4, 10, INK[2])
        for k in range(n):
            x = 4 + ci * 16 + (k % 2) * 6
            y = 7 + (k // 2) * 7 + (k % 2)
            c = cols[(ci * 2 + k) % 5]
            rect(im, x, y, 6, 5, c)
            hline(im, x + 1, y + 2, 3, alpha(INK[2], 110))
            px(im, x + 2, y, C["red"][3])
    # markers on the tray
    for i, c in enumerate((C["red"][3], C["blue"][3], C["green"][3])):
        rect(im, 6 + i * 6, h - 3, 4, 1, c)
    ol(im)
    return im


def painting():
    w, h = 30, 22
    im = new(w, h)
    rect(im, 0, 0, w, h, C["yellow"][2])
    frame_rect(im, 0, 0, w, h, C["yellow"][1])
    hline(im, 0, 0, w, C["yellow"][4])
    rect(im, 3, 3, w - 6, h - 6, hexc("#f2c08a"))
    rect(im, 3, 3, w - 6, 6, hexc("#f6d8a4"))
    ellipse(im, 20, 8, 3, 3, hexc("#fbefc0"))
    for x in range(3, w - 3):
        y = int(11 + math.sin(x * 0.4) * 2)
        vline(im, x, y, h - 3 - y, C["teal"][2])
        y2 = int(14 + math.sin(x * 0.3 + 2) * 1.5)
        vline(im, x, y2, h - 3 - y2, C["teal"][1])
    rect(im, 3, h - 5, w - 6, 2, C["sage"][1])
    ol(im)
    return im


def poster():
    w, h = 20, 28
    im = new(w, h)
    rect(im, 0, 0, w, h, hexc("#f0e6d0"))
    rect(im, 2, 2, w - 4, 16, hexc("#2a2440"))
    cx, cy = 10, 11
    for k in range(8):
        a = k * math.pi / 4
        line(im, cx, cy, int(cx + math.cos(a) * 6), int(cy + math.sin(a) * 6), C["orange"][3])
    ellipse(im, cx, cy, 2.5, 2.5, C["orange"][4])
    for y, ww in ((21, 14), (24, 10)):
        hline(im, (w - ww) // 2, y, ww, INK[2])
    px(im, 2, 0, ST[5])
    px(im, w - 3, 0, ST[5])
    ol(im)
    return im


def wall_shelf():
    w, h = 40, 18
    im = new(w, h)
    r = C["oak"]
    rect(im, 0, 13, w, 3, r[3])
    hline(im, 0, 13, w, r[4])
    hline(im, 0, 15, w, r[1])
    px(im, 4, 16, ST[3])
    px(im, w - 5, 16, ST[3])
    # trophies + books + plant
    x = 2
    for c, hh in ((C["red"][3], 9), (C["blue"][3], 10), (C["cream"][3], 8), (C["teal"][3], 10)):
        rect(im, x, 13 - hh, 3, hh, c)
        px(im, x, 13 - hh, alpha(WHITE, 80))
        x += 3
    rect(im, 17, 7, 5, 4, C["yellow"][3])
    hline(im, 18, 11, 3, C["yellow"][2])
    rect(im, 19, 11, 1, 2, C["yellow"][2])
    px(im, 18, 8, C["yellow"][4])
    pot(im, 28, 9, 6, 4, C["cream"])
    for (a, b) in ((0, -2), (2, -4), (4, -3), (6, -1), (1, -1), (3, -2), (-1, 0), (7, 1), (7, 3), (8, 5)):
        px(im, 28 + a, 9 + b, C["green"][4] if b < -2 else C["green"][3])
    ol(im)
    return im


def julius_door(state):
    """34x56. state 0 closed, 1 ajar, 2 open."""
    w, h = 34, 56
    im = new(w, h)
    fr = C["walnut"]
    rect(im, 0, 4, w, h - 4, fr[1])
    hline(im, 0, 4, w, fr[3])
    vline(im, 0, 4, h - 4, fr[3])
    # transom
    rect(im, 3, 7, w - 6, 5, hexc("#f6d890"))
    hline(im, 3, 7, w - 6, hexc("#fbefc0"))
    vline(im, w // 2, 7, 5, fr[1])
    dx0, dy0, dw, dh = 3, 13, w - 6, h - 13
    inner = hexc("#ffd890")
    if state > 0:
        # warm room light behind
        rect(im, dx0, dy0, dw, dh, inner)
        for y in range(dy0, dy0 + dh):
            if y % 3 == 0:
                hline(im, dx0, y, dw, hexc("#f6c870"))
        rect(im, dx0, dy0 + dh - 6, dw, 6, hexc("#c89060"))
    d = C["teal"]
    if state == 0:
        rect(im, dx0, dy0, dw, dh, d[2])
        hline(im, dx0, dy0, dw, d[3])
        vline(im, dx0, dy0, dh, d[3])
        frame_rect(im, dx0 + 3, dy0 + 4, dw - 6, 14, d[1])
        frame_rect(im, dx0 + 3, dy0 + 22, dw - 6, 17, d[1])
        hline(im, dx0 + 4, dy0 + 5, dw - 8, d[3])
        hline(im, dx0 + 4, dy0 + 23, dw - 8, d[3])
        # brass knob + plate
        rect(im, dx0 + dw - 5, dy0 + 20, 2, 3, C["yellow"][3])
        px(im, dx0 + dw - 5, dy0 + 20, C["yellow"][4])
    else:
        ww = 7 if state == 1 else 3
        rect(im, dx0, dy0, ww, dh, d[1])
        vline(im, dx0 + ww - 1, dy0, dh, d[0])
        vline(im, dx0, dy0, dh, d[3])
        if state == 1:
            px(im, dx0 + ww - 2, dy0 + 20, C["yellow"][3])
    # name plate; the name itself is drawn by OfficeView (office.doorName)
    plate_y = 1
    rect(im, 3, plate_y, w - 6, 7, C["yellow"][2])
    hline(im, 3, plate_y, w - 6, C["yellow"][4])
    hline(im, 3, plate_y + 6, w - 6, C["yellow"][1])
    ol(im)
    return im


FONT3 = {
    "J": ["..#", "..#", "..#", "#.#", ".#."],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "S": [".##", "#..", ".#.", "..#", "##."],
    "E": ["###", "#..", "##.", "#..", "###"],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "O": [".#.", "#.#", "#.#", "#.#", ".#."],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "N": ["#.#", "###", "###", "#.#", "#.#"],
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "!": [".#.", ".#.", ".#.", "...", ".#."],
}


def draw_text3x5(im, x, y, s, c):
    for ch in s:
        g = FONT3.get(ch)
        if g:
            stamp(im, x, y, g, {"#": c})
        x += 4


def door_lamp(state):
    w, h = 8, 6
    im = new(w, h)
    rect(im, 0, 0, w, 2, ST[3])
    col = {0: ST[2], 1: C["red"][4], 2: C["code"][0]}[state]
    ellipse(im, 4, 3, 3, 2.5, col)
    if state:
        px(im, 3, 2, alpha(WHITE, 160))
    ol(im)
    return im


def wall_lamp_glow(col):
    im = new(40, 30)
    for (rx, ry, a) in ((18, 13, 22), (12, 9, 30), (7, 5, 44)):
        ellipse(im, 20, 10, rx, ry, alpha(col, a))
    return im


def exit_sign():
    im = new(18, 8)
    rect(im, 0, 0, 18, 8, C["green"][2])
    hline(im, 0, 0, 18, C["green"][4])
    draw_text3x5(im, 2, 2, "EXIT", WHITE)
    ol(im)
    return im


def printer(frame):
    w, h = 28, 30
    im = new(w, h)
    # stand cabinet
    rect(im, 2, 14, 24, 14, ST[3])
    hline(im, 2, 14, 24, ST[5])
    hline(im, 3, 20, 22, ST[2])
    rect(im, 12, 17, 4, 1, ST[1])
    rect(im, 12, 23, 4, 1, ST[1])
    # printer body
    rect(im, 0, 4, w, 11, ST[6])
    hline(im, 0, 4, w, ST[7])
    vline(im, w - 1, 5, 10, ST[4])
    rect(im, 3, 0, 22, 5, ST[5])
    hline(im, 3, 0, 22, ST[7])
    rect(im, 18, 6, 6, 3, INK[2])
    px(im, 19, 7, C["code"][0] if frame else C["teal"][2])
    px(im, 22, 7, C["sky"][3])
    rect(im, 4, 11, 12, 2, INK[2])
    # paper coming out
    if frame:
        rect(im, 5, 8 + frame, 10, 4, WHITE)
        hline(im, 6, 9 + frame, 6, ST[5])
    else:
        rect(im, 5, 11, 10, 1, WHITE)
    rect(im, 2, 28, 24, 2, INK[2])
    ol(im)
    return im


def whiteboard_stand():
    w, h = 44, 44
    im = new(w, h)
    # legs + wheels
    vline(im, 4, 26, 15, ST[4])
    vline(im, 39, 26, 15, ST[3])
    hline(im, 1, 40, 8, ST[3])
    hline(im, 35, 40, 8, ST[3])
    for x in (1, 8, 35, 42):
        px(im, x, 42, INK[1])
    rect(im, 0, 0, w, 28, ST[5])
    rect(im, 2, 2, w - 4, 24, hexc("#f6f6f2"))
    hline(im, 0, 0, w, ST[7])
    # diagram: boxes + arrows + a sketch
    for (x, y, c) in ((5, 5, C["blue"][3]), (18, 5, C["red"][3]), (31, 5, C["green"][3])):
        frame_rect(im, x, y, 9, 6, c)
    line(im, 14, 8, 17, 8, INK[2])
    line(im, 27, 8, 30, 8, INK[2])
    line(im, 22, 11, 22, 16, INK[2])
    frame_rect(im, 17, 16, 11, 6, C["orange"][2])
    for x in range(5, 14):
        px(im, x, 20 - ((x * 3) % 5), C["blue"][3])
    hline(im, 30, 18, 8, ST[4])
    hline(im, 30, 20, 6, ST[4])
    rect(im, 2, 26, w - 4, 2, ST[3])
    rect(im, 6, 26, 4, 1, C["red"][3])
    ol(im)
    return im


def coat_rack():
    w, h = 18, 44
    im = new(w, h)
    vline(im, 8, 4, 36, C["walnut"][2])
    vline(im, 9, 4, 36, C["walnut"][1])
    hline(im, 3, 41, 12, C["walnut"][1])
    ellipse(im, 9, 3, 2, 2, C["walnut"][3])
    for (x, y) in ((5, 6), (13, 6)):
        line(im, 8, 8, x, y, C["walnut"][2])
    # coat + scarf + bag
    rect(im, 2, 8, 6, 16, C["blue"][2])
    vline(im, 2, 8, 16, C["blue"][3])
    rect(im, 11, 8, 4, 10, C["red"][3])
    vline(im, 12, 18, 4, C["red"][3])
    rect(im, 10, 22, 7, 6, C["oak"][2])
    line(im, 10, 22, 13, 9, C["oak"][1])
    ol(im)
    return im


def umbrella_stand():
    im = new(10, 18)
    rect(im, 1, 8, 8, 9, C["sage"][2])
    hline(im, 1, 8, 8, C["sage"][4])
    vline(im, 4, 0, 9, C["red"][3])
    px(im, 3, 0, C["red"][3])
    vline(im, 6, 2, 7, INK[2])
    px(im, 7, 2, INK[2])
    ol(im)
    return im


def trash_bin():
    im = new(10, 12)
    rect(im, 1, 2, 8, 9, ST[4])
    hline(im, 1, 2, 8, ST[6])
    for x in (3, 5, 7):
        vline(im, x, 4, 6, ST[3])
    rect(im, 3, 0, 4, 3, WHITE)
    ol(im)
    return im


def cat_bed():
    im = new(22, 12)
    ellipse(im, 11, 6, 10.5, 5.5, C["red"][2])
    ellipse(im, 11, 5, 9.5, 4.5, C["red"][3])
    ellipse(im, 11, 6.5, 7, 3, C["cream"][3])
    ellipse(im, 10.5, 6, 6, 2.4, C["cream"][4])
    ol(im)
    return im


def mat(w, h, ramp, pattern):
    im = new(w, h)
    r = C[ramp]
    rect(im, 0, 0, w, h, r[1])
    rect(im, 1, 1, w - 2, h - 2, r[2])
    if pattern == "stripes":
        for x in range(3, w - 3, 3):
            vline(im, x, 2, h - 4, r[3])
    else:
        for y in range(2, h - 2, 2):
            for x in range(2 + (y // 2) % 2, w - 2, 2):
                px(im, x, y, r[3])
    frame_rect(im, 0, 0, w, h, r[0])
    return im


def rug(w, h, base, border, accent, seed):
    im = new(w, h)
    b, bd, ac = C[base], C[border], C[accent]
    rect(im, 0, 0, w, h, bd[1])
    rect(im, 2, 2, w - 4, h - 4, b[2])
    frame_rect(im, 4, 4, w - 8, h - 8, ac[3])
    frame_rect(im, 6, 6, w - 12, h - 12, bd[2])
    # woven pattern: diamonds
    cx, cy = w / 2, h / 2
    for y in range(8, h - 8):
        for x in range(8, w - 8):
            d = abs(x - cx) / (w / 2 - 8) + abs(y - cy) / (h / 2 - 8)
            if 0.55 < d < 0.62 or 0.22 < d < 0.28:
                px(im, x, y, ac[3])
            elif d < 0.12:
                px(im, x, y, bd[2])
            elif (x + y) % 6 == 0 and d > 0.65:
                px(im, x, y, b[1])
    # fringe
    for x in range(2, w - 2, 2):
        px(im, x, 0, C["cream"][4])
        px(im, x, h - 1, C["cream"][4])
    frame_rect(im, 1, 1, w - 2, h - 2, bd[0])
    return im


def carpet(w, h):
    """Low-pile carpet tiles that zone the work area."""
    im = new(w, h)
    a, b, edge = hexc("#5f7866"), hexc("#58705f"), hexc("#3b5a50")
    for y in range(h):
        for x in range(w):
            tx, ty = x // 12, y // 10
            c = a if (tx + ty) % 2 == 0 else b
            # alternating pile direction
            if (tx + ty) % 2 == 0 and (x + 2 * y) % 5 == 0:
                c = hexc("#668070")
            if (tx + ty) % 2 == 1 and (2 * x + y) % 5 == 0:
                c = hexc("#52695a")
            px(im, x, y, c)
    for x in range(w):
        px(im, x, 0, hexc("#86a48e"))
        px(im, x, h - 1, edge)
        px(im, x, h - 2, hexc("#4a6656"))
    for y in range(h):
        px(im, 0, y, hexc("#7a9a84"))
        px(im, w - 1, y, edge)
    return im


# --------------------------------------------------------------------- cat --

CAT = [hexc("#7a3326"), hexc("#b0532c"), hexc("#e08a3e"), hexc("#f6b868")]


def cat_side(phase, mode="walk"):
    w, h = 18, 13
    im = new(w, h)
    k, d, m, l = CAT[1], CAT[0], CAT[2], CAT[3]
    wh = WHITE
    base = 9
    bob = 1 if (mode == "walk" and phase % 2 == 1) else 0
    # tail
    tail = [(3, 4), (2, 3), (2, 2), (3, 1)] if mode != "flick" else [(3, 4), (2, 3), (1, 2), (1, 1)] if phase % 2 else [(3, 4), (3, 3), (4, 2), (5, 1)]
    if mode == "walk":
        tail = [(3, 5), (2, 4), (2, 3), (2 + (phase % 2), 2)]
    for (x, y) in tail:
        px(im, x, y + bob, m)
        px(im, x + 1, y + bob, k)
    # body
    rect(im, 4, 5 + bob, 9, 4, m)
    hline(im, 4, 5 + bob, 9, l)
    hline(im, 5, 8 + bob, 7, k)
    for sx in (6, 8, 10):
        px(im, sx, 5 + bob, k)
        px(im, sx, 6 + bob, k)
    # head
    rect(im, 12, 2 + bob, 5, 5, m)
    hline(im, 12, 2 + bob, 5, l)
    px(im, 12, 1 + bob, m)
    px(im, 15, 1 + bob, m)
    px(im, 12, 0 + bob, k)
    px(im, 15, 0 + bob, k)
    px(im, 15, 3 + bob, C["yellow"][3])
    px(im, 16, 5 + bob, C["pink"][3])
    px(im, 13, 6 + bob, wh)
    px(im, 14, 6 + bob, wh)
    # legs
    if mode == "walk":
        offs = [(0, 1, 2, 3), (1, 0, 3, 2), (2, 3, 0, 1), (3, 2, 1, 0)][phase % 4]
        legs = [5, 6, 10, 11]
        for i, x in enumerate(legs):
            o = offs[i]
            xx = x + (1 if o == 0 else -1 if o == 2 else 0)
            col = k if i in (1, 3) else m
            vline(im, xx, 9 + bob, 3 - bob, col)
            px(im, xx, 11, wh)
    else:
        for x in (5, 7, 10, 12):
            vline(im, x, 9, 2, m if x in (5, 10) else k)
            px(im, x, 11, wh)
    outline(im, CAT[0] if False else OUTLINE)
    return im


def cat_sit(phase):
    w, h = 18, 13
    im = new(w, h)
    k, d, m, l = CAT[1], CAT[0], CAT[2], CAT[3]
    stamp(im, 5, 0, [
        ".m...m..",
        ".mm.mm..",
        ".mmmmm..",
        ".mEmEm..",
        ".mmPmm..",
        "..mWm...",
        ".mmWmm..",
        "mmmWmmm.",
        "mmmmmmm.",
        "mWmmmWm.",
        "WW...WW.",
    ], {"m": m, "E": C["yellow"][3], "P": C["pink"][3], "W": WHITE})
    hline(im, 6, 2, 5, l)
    px(im, 8, 2, k)
    px(im, 5, 8, k)
    px(im, 11, 8, k)
    px(im, 6, 1, C["pink"][2])
    px(im, 10, 1, C["pink"][2])
    if phase == 2:
        px(im, 7, 3, k)
        px(im, 9, 3, k)
    # tail curling right; flicks
    tails = [[(12, 10), (13, 10), (14, 9), (15, 8)], [(12, 10), (13, 10), (14, 10), (15, 9), (16, 8)],
             [(12, 10), (13, 10), (14, 9), (15, 8)], [(12, 10), (13, 9), (14, 8), (14, 7)]]
    for (x, y) in tails[phase % 4]:
        px(im, x, y, m)
    outline(im, OUTLINE)
    return im


def cat_sleep(phase):
    w, h = 18, 13
    im = new(w, h)
    k, d, m, l = CAT[1], CAT[0], CAT[2], CAT[3]
    ellipse(im, 9, 8.5 - phase * 0.5, 7, 3.6 + phase * 0.4, m)
    ellipse(im, 8, 7.5, 5, 2, l)
    # tail wrapped around
    for x in range(4, 13):
        px(im, x, 11, k)
    px(im, 13, 10, k)
    # head tucked on the right
    ellipse(im, 13, 7, 3, 2.6, m)
    px(im, 12, 4, m)
    px(im, 15, 5, m)
    hline(im, 12, 7, 2, k)
    px(im, 15, 8, C["pink"][3])
    px(im, 14, 9, WHITE)
    outline(im, OUTLINE)
    return im


def cat_sheet():
    s = Sheet("cat", 18, 13, anchor=(9, 12), kind="character")
    walk = [s.add(cat_side(i)) for i in range(4)]
    walk_l = [s.add(mirror(cat_side(i))) for i in range(4)]
    stand = [s.add(cat_side(i, "flick")) for i in range(2)]
    stand_l = [s.add(mirror(cat_side(i, "flick"))) for i in range(2)]
    sit = [s.add(cat_sit(i)) for i in range(4)]
    sleep = [s.add(cat_sleep(i)) for i in range(2)]
    s.anim("walk_right", walk, 8)
    s.anim("walk_left", walk_l, 8)
    s.anim("stand_right", [stand[0]] * 6 + [stand[1], stand[0], stand[1]], 4)
    s.anim("stand_left", [stand_l[0]] * 6 + [stand_l[1], stand_l[0], stand_l[1]], 4)
    s.anim("sit", [sit[0]] * 5 + [sit[1], sit[0], sit[3], sit[0], sit[2]] + [sit[0]] * 3, 4)
    s.anim("tail_flick", [sit[0], sit[1], sit[0], sit[3]], 6)
    s.anim("sleep", [sleep[0], sleep[0], sleep[1], sleep[1]], 2)
    s.save()
    return s
