# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The static room: floor materials, walls, trims. One 480x270 image."""

from .core import C, T, OUTLINE, WHITE, hexc, alpha, new, px, rect, hline, vline, frame_rect, rng, line

W, H = 480, 270
CAP = 6            # wall-top thickness (top, left, right)
FLOOR_Y = 60       # first floor row
BOTTOM_CAP = 262   # bottom wall-top starts here
ENTRANCE = (100, 134)  # gap in the bottom wall (x0, x1)
KITCHEN_FLOOR = (56, 186, 60, 104)
SERVER_FLOOR = (6, 64, 60, 104)


def planks(im):
    p = C["plank"]
    R = rng(1234)
    y = FLOOR_Y
    row = 0
    while y < BOTTOM_CAP:
        hgt = 6
        x = -R.randrange(0, 40)
        while x < W:
            L = R.randrange(28, 62)
            tone = R.choice((3, 3, 4, 4, 3, 2))
            base = p[tone]
            rect(im, max(0, x), y, min(L, W - max(0, x)) if x >= 0 else L + x, hgt - 1, base)
            # top highlight on lighter planks, grain streaks
            for k in range(R.randrange(1, 3)):
                gx = x + R.randrange(2, max(3, L - 8))
                gy = y + R.randrange(1, hgt - 1)
                gl = R.randrange(4, 12)
                hline(im, gx, gy, gl, p[tone - 1])
            if R.random() < 0.18:
                kx = x + R.randrange(4, max(5, L - 4))
                ky = y + R.randrange(1, hgt - 2)
                px(im, kx, ky, p[1])
                px(im, kx + 1, ky, p[tone - 1])
            if R.random() < 0.2:
                hline(im, x + 1, y, max(1, L // 4), p[min(5, tone + 1)])
            # end joint
            vline(im, x + L - 1, y, hgt - 1, p[1])
            x += L
        hline(im, 0, y + hgt - 1, W, p[1])
        y += hgt
        row += 1


def tiles(im, x0, x1, y0, y1, a, b, tw=8, th=6, grout=None):
    for y in range(y0, y1):
        for x in range(x0, x1):
            i, j = (x - x0) // tw, (y - y0) // th
            c = a if (i + j) % 2 == 0 else b
            if grout and ((x - x0) % tw == 0 or (y - y0) % th == 0):
                c = grout
            px(im, x, y, c)


def build():
    im = new(W, H)
    planks(im)
    # kitchen: warm checker tiles
    kx0, kx1, ky0, ky1 = KITCHEN_FLOOR
    tiles(im, kx0, kx1, ky0, ky1, C["cream"][3], hexc("#c9a98a"), 10, 7)
    for x in range(kx0, kx1):
        px(im, x, ky1 - 1, C["plank"][1])
    hline(im, kx0, ky1, kx1 - kx0, C["oak"][1])
    # server corner: raised floor panels
    sx0, sx1, sy0, sy1 = SERVER_FLOOR
    tiles(im, sx0, sx1, sy0, sy1, C["steel"][4], C["steel"][4], 12, 9, C["steel"][3])
    for y in range(sy0 + 1, sy1, 9):
        for x in range(sx0 + 1, sx1, 12):
            px(im, x + 1, y + 1, C["steel"][5])
            px(im, x + 10, y + 7, C["steel"][2])
    # perforated panel in front of the rack
    for y in range(sy0 + 30, sy0 + 37):
        for x in range(sx0 + 13, sx0 + 23):
            if (x + y) % 2 == 0:
                px(im, x, y, C["steel"][2])
    vline(im, sx1, sy0, sy1 - sy0, C["steel"][2])
    hline(im, sx0, sy1, sx1 - sx0 + 1, C["steel"][2])

    wall(im)
    # ambient occlusion along the walls
    for i, a in enumerate((90, 55, 30, 14)):
        hline(im, CAP, FLOOR_Y + i, W - 2 * CAP, (30, 20, 40, a))
        vline(im, CAP + i, FLOOR_Y, BOTTOM_CAP - FLOOR_Y, (30, 20, 40, a))
        vline(im, W - CAP - 1 - i, FLOOR_Y, BOTTOM_CAP - FLOOR_Y, (30, 20, 40, a))
    for i, a in enumerate((60, 30, 12)):
        hline(im, CAP, BOTTOM_CAP - 1 - i, W - 2 * CAP, (30, 20, 40, a))
    caps(im)
    # rubber cable cover: server corner -> pod A
    for y in range(105, 160):
        px(im, 61, y, C["steel"][2])
        px(im, 62, y, C["steel"][4])
        px(im, 63, y, C["steel"][3])
        px(im, 64, y, C["steel"][2])
        if y % 6 == 0:
            px(im, 62, y, C["yellow"][2])
            px(im, 63, y, C["yellow"][2])
    # floor boxes under the pods (cable outlets)
    for (x, y) in ((60, 160), (172, 168)):
        rect(im, x, y, 6, 4, C["steel"][4])
        hline(im, x, y, 6, C["steel"][6])
        px(im, x + 2, y + 2, C["steel"][2])
    # wall outlets and switches
    for x in (150, 238, 330, 460):
        rect(im, x, 50, 4, 3, C["cream"][4])
        px(im, x + 1, 51, C["steel"][3])
        px(im, x + 2, 51, C["steel"][3])
    for x in (386, 176):
        rect(im, x, 26, 3, 5, C["cream"][4])
        px(im, x + 1, 27, C["steel"][4])
        px(im, x + 1, 29, C["steel"][5])
    return im


def wall(im):
    cr = C["cream"]
    sg = C["sage"]
    wd = C["walnut"]
    # plaster with a faint wallpaper motif
    rect(im, 0, CAP, W, FLOOR_Y - CAP, cr[3])
    for y in range(CAP + 3, 34, 6):
        for x in range((y // 6) % 2 * 4, W, 8):
            px(im, x, y, cr[2])
    hline(im, 0, CAP, W, cr[1])
    hline(im, 0, CAP + 1, W, cr[2])
    # chair rail
    hline(im, 0, 34, W, wd[3])
    hline(im, 0, 35, W, wd[2])
    hline(im, 0, 36, W, wd[1])
    # wainscot panels
    rect(im, 0, 37, W, 17, sg[2])
    for x in range(CAP + 2, W, 22):
        frame_rect(im, x, 39, 18, 13, sg[1])
        hline(im, x + 1, 40, 16, sg[3])
        vline(im, x + 1, 40, 11, sg[3])
    hline(im, 0, 37, W, sg[1])
    # baseboard
    rect(im, 0, 54, W, 6, wd[2])
    hline(im, 0, 54, W, wd[4])
    hline(im, 0, 55, W, wd[3])
    hline(im, 0, 59, W, wd[0])
    # kitchen backsplash: subway tiles
    for y in range(30, 54):
        for x in range(58, 132):
            yy = (y - 30) // 4
            off = 3 if yy % 2 else 0
            c = WHITE if (y - 30) % 4 else C["cream"][2]
            if (x - 58 + off) % 7 == 0:
                c = C["cream"][2]
            px(im, x, y, c)
    hline(im, 58, 30, 74, C["cream"][1])
    # server corner: darker accent wall + cable tray
    rect(im, CAP, CAP, 50, 48, hexc("#3c4658"))
    for y in range(CAP + 2, 54, 4):
        for x in range(CAP + (y // 4) % 2 * 2, 56, 4):
            px(im, x, y, hexc("#465266"))
    rect(im, CAP, 12, 50, 4, C["steel"][3])
    hline(im, CAP, 12, 50, C["steel"][5])
    hline(im, CAP, 15, 50, C["steel"][1])
    for i, c in enumerate((C["blue"][3], C["yellow"][2], C["red"][2], C["teal"][3])):
        line(im, 20 + i * 2, 16, 20 + i * 2, 26, c)
    hline(im, CAP, 53, 50, hexc("#2c3444"))


def caps(im):
    ink = C["ink"]
    # top
    rect(im, 0, 0, W, CAP, ink[2])
    hline(im, 0, CAP - 1, W, ink[3])
    hline(im, 0, 0, W, ink[1])
    # sides
    rect(im, 0, 0, CAP, H, ink[2])
    rect(im, W - CAP, 0, CAP, H, ink[2])
    vline(im, CAP - 1, CAP, H - CAP, ink[3])
    vline(im, W - CAP, CAP, H - CAP, ink[3])
    vline(im, 0, 0, H, ink[1])
    vline(im, W - 1, 0, H, ink[1])
    # side wall faces are seen edge-on: a thin light trim where they meet the back wall
    for y in range(CAP, FLOOR_Y):
        px(im, CAP, y, C["cream"][1])
        px(im, W - CAP - 1, y, C["cream"][1])
    # bottom wall top with the entrance gap
    x0, x1 = ENTRANCE
    for (a, b) in ((0, x0), (x1, W)):
        rect(im, a, BOTTOM_CAP, b - a, H - BOTTOM_CAP, ink[2])
        hline(im, a, BOTTOM_CAP, b - a, ink[3])
        hline(im, a, H - 1, b - a, ink[1])
    # entrance threshold + hallway floor beyond
    rect(im, x0, BOTTOM_CAP, x1 - x0, H - BOTTOM_CAP, C["steel"][3])
    hline(im, x0, BOTTOM_CAP, x1 - x0, C["steel"][5])
    for x in range(x0, x1, 4):
        vline(im, x, BOTTOM_CAP + 2, H - BOTTOM_CAP - 2, C["steel"][2])
    vline(im, x0 - 1, BOTTOM_CAP, H - BOTTOM_CAP, ink[0])
    vline(im, x1, BOTTOM_CAP, H - BOTTOM_CAP, ink[0])
    # door jambs
    rect(im, x0 - 3, BOTTOM_CAP - 2, 3, H - BOTTOM_CAP + 2, C["walnut"][3])
    rect(im, x1, BOTTOM_CAP - 2, 3, H - BOTTOM_CAP + 2, C["walnut"][2])
