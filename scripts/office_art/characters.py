# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Office people: a small pixel puppet posed frame by frame.

Frame 24x32, anchor (12, 31) = the floor point under the feet (or under the
chair seat for seated poses). Light comes from the top left.
"""

from .core import C, T, hexc, new, px, rect, outline, mirror, Sheet, OUTLINE

FW, FH = 24, 32
ANCHOR = (12, 31)

HAIR = {
    "dark":   ["#22161c", "#38242a", "#553630", "#74503e"],
    "brown":  ["#3a2018", "#5c3424", "#82522f", "#a8703e"],
    "blonde": ["#7a5224", "#b8873a", "#e2b85a", "#f6dc92"],
    "black":  ["#14111a", "#211c2a", "#332b40", "#4b405a"],
    "ginger": ["#5e2416", "#983e22", "#cc6630", "#eca050"],
    "grey":   ["#5e5a68", "#8e8a98", "#c0bec8", "#eae8ee"],
}
HAIR = {k: [hexc(c) for c in v] for k, v in HAIR.items()}

LOOKS = [
    # 0: the lead. Navy blazer, white shirt, red tie, headset.
    dict(skin="skin0", hair="dark", style="part", top="blazer", cloth="blue",
         cl=(0, 1, 2), shirt=C["steel"][7], accent=C["red"][3], pants=C["steel"][1:4],
         shoes=C["ink"][1], headset=True),
    dict(skin="skin1", hair="brown", style="curly", top="hoodie", cloth="orange",
         cl=(1, 2, 3), accent=C["orange"][4], pants=C["blue"][1:4], shoes=C["steel"][6]),
    dict(skin="skin0", hair="blonde", style="bob", top="sweater", cloth="sage",
         cl=(2, 3, 4), accent=C["cream"][4], pants=C["steel"][1:4], shoes=C["wood"][1]),
    dict(skin="skin2", hair="black", style="bun", top="shirt", cloth="purple",
         cl=(2, 3, 4), accent=C["purple"][4], pants=C["oak"][1:4], shoes=C["ink"][1], glasses=True),
    dict(skin="skin0", hair="ginger", style="beanie", top="flannel", cloth="red",
         cl=(1, 2, 3), accent=C["ink"][2], pants=C["blue"][0:3], shoes=C["wood"][1],
         hat=C["teal"][1:4], beard=True, freckles=True),
    dict(skin="skin3", hair="black", style="ponytail", top="tee", cloth="teal",
         cl=(2, 3, 4), accent=C["yellow"][3], pants=C["blue"][1:4], shoes=C["steel"][6]),
    dict(skin="skin1", hair="grey", style="short", top="cardigan", cloth="yellow",
         cl=(1, 2, 3), shirt=C["cream"][4], accent=C["oak"][1], pants=C["walnut"][1:4],
         shoes=C["ink"][1], glasses=True),
    dict(skin="skin2", hair="black", style="spiky", top="tee", cloth="pink",
         cl=(1, 2, 3), accent=C["steel"][6], pants=C["green"][1:4], shoes=C["steel"][6],
         phones=True),
]


class Look:
    def __init__(self, d):
        self.d = d
        s = C[d["skin"]]
        self.skin = s  # 0 dark .. 3 light
        self.hair = HAIR[d["hair"]]
        r = C[d["cloth"]]
        a, b, c = d["cl"]
        self.cloth = (r[a], r[b], r[c])  # shadow, mid, light
        self.pants = d["pants"]
        self.shoes = d["shoes"]
        self.accent = d.get("accent")
        self.shirt = d.get("shirt", C["steel"][7])

    def get(self, k, default=None):
        return self.d.get(k, default)


# ------------------------------------------------------------------ head --

FRONT_HEAD = [
    "..SSSSSSSS..",
    ".SSSSSSSSSS.",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSSs",
    ".SSSSSSSSSs.",
    ".sSSSSSSSss.",
    "...ssssss...",
]
SIDE_HEAD = [
    "..SSSSSS...",
    ".SSSSSSSS..",
    "SSSSSSSSSS.",
    "SSSSSSSSSSS",
    "SSSSSSSSSSS",
    "SSSSSSSSSSS",
    "SSSSSSSSSSSS",
    "SSSSSSSSSSS",
    ".SSSSSSSSSS",
    ".sSSSSSSSs.",
    "...ssssss..",
]


def head_mask(rows):
    return {(x, y) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch != "."}


FM = head_mask(FRONT_HEAD)
SM = head_mask(SIDE_HEAD)


def span(y, x0, x1):
    return {(x, y) for x in range(x0, x1 + 1)}


def hair_front(style):
    h = set()
    if style in ("part", "short", "ponytail", "bun", "bob", "curly", "spiky"):
        h |= span(-1, 2, 9) | span(0, 1, 10) | span(1, 0, 11) | span(2, 0, 11)
    if style == "part":
        h |= span(3, 0, 6) | span(4, 0, 2) | {(0, 5), (11, 3), (11, 4)}
        h -= {(7, 0)}
    elif style == "short":
        h |= span(3, 0, 3) | span(3, 8, 11) | {(0, 4), (11, 4), (4, 3)}
    elif style == "curly":
        h |= span(-2, 2, 3) | span(-2, 5, 6) | span(-2, 8, 9)
        h |= span(3, 0, 11) - {(2, 3), (6, 3), (9, 3)}
        for y in range(1, 8):
            h |= {(-1, y), (12, y)} if y % 3 != 0 else set()
        h |= {(0, y) for y in range(3, 7)} | {(11, y) for y in range(3, 7)}
    elif style == "bob":
        h |= span(3, 0, 11) - {(5, 3), (6, 3)}
        for y in range(1, 10):
            h |= {(-1, y), (0, y), (12, y), (11, y)}
        h |= {(1, 4), (10, 4), (1, 9), (10, 9)}
    elif style in ("bun", "ponytail"):
        h |= span(3, 0, 4) | span(3, 9, 11) | {(0, 4), (11, 4), (0, 5), (11, 5)}
        if style == "bun":
            h |= span(-4, 4, 7) | span(-3, 3, 8) | span(-2, 3, 8)
        else:
            h |= {(12, y) for y in range(4, 10)} | {(13, y) for y in range(6, 9)}
    elif style == "spiky":
        h |= {(2, -2), (4, -3), (4, -2), (6, -2), (7, -3), (9, -2), (1, -1), (10, -1)}
        h |= span(3, 2, 9) - {(4, 3), (8, 3)}
    elif style == "beanie":
        h |= {(0, 4), (11, 4), (0, 5), (11, 5)}
    return h


def hair_back(style):
    h = set()
    for (x, y) in FM:
        if y <= 7:
            h.add((x, y))
    h |= span(-1, 2, 9)
    if style == "bob":
        h |= {(x, y) for (x, y) in FM if y <= 9}
        for y in range(1, 10):
            h |= {(-1, y), (12, y)}
    elif style == "curly":
        h |= span(-2, 2, 3) | span(-2, 5, 6) | span(-2, 8, 9) | {(x, 8) for x in range(1, 11)}
        for y in range(1, 8):
            if y % 3 != 0:
                h |= {(-1, y), (12, y)}
    elif style == "bun":
        h |= span(-4, 4, 7) | span(-3, 3, 8) | span(-2, 3, 8)
    elif style == "ponytail":
        h |= {(x, y) for x in range(4, 8) for y in range(8, 13)} | span(13, 5, 6)
    elif style == "spiky":
        h |= {(2, -2), (4, -3), (4, -2), (6, -2), (7, -3), (9, -2), (1, -1), (10, -1)}
    elif style in ("short", "part"):
        h -= span(7, 0, 11) - span(7, 2, 9)
    return h


def hair_side(style):
    """Facing right: face on the right, back of the head on the left."""
    h = set()
    if style == "beanie":
        return {(x, y) for (x, y) in SM if x <= 2 and 3 <= y <= 6}
    h |= span(-1, 2, 7) | span(0, 1, 8) | span(1, 0, 9) | span(2, 0, 10)
    back = {3: 6, 4: 4, 5: 3, 6: 3, 7: 3, 8: 3}
    if style == "bob":
        back = {3: 6, 4: 5, 5: 4, 6: 4, 7: 4, 8: 4, 9: 4}
        h |= {(-1, y) for y in range(1, 10)}
    for y, xm in back.items():
        h |= {(x, y) for x in range(0, xm + 1) if (x, y) in SM or x == 0}
    if style in ("part", "short"):
        h |= {(9, 3), (10, 3)} if style == "part" else {(9, 3)}
    elif style == "curly":
        h |= span(-2, 2, 3) | span(-2, 5, 6) | {(-1, 2), (-1, 3), (-1, 5), (-1, 6), (10, 3), (8, 3)}
    elif style == "bob":
        h |= span(3, 6, 10)
    elif style == "bun":
        h |= span(-4, 2, 4) | span(-3, 1, 5) | span(-2, 1, 5) | {(9, 3)}
    elif style == "ponytail":
        h |= {(-1, y) for y in range(4, 11)} | {(-2, y) for y in range(6, 11)} | {(-1, 11), (-2, 11)}
        h |= {(9, 3)}
    elif style == "spiky":
        h |= {(2, -2), (4, -3), (4, -2), (6, -2), (7, -2), (8, -1), (9, 2), (10, 2)}
    return h


def shade_set(cells, ramp, cx):
    """mid / highlight top-left / shadow bottom-right."""
    out = {}
    for (x, y) in cells:
        c = ramp[2]
        if (x, y - 1) not in cells and x <= cx:
            c = ramp[3]
        elif (x, y + 1) not in cells or (x + 1, y) not in cells:
            c = ramp[1]
        out[(x, y)] = c
    return out


def draw_head(im, lk, facing, ox, oy, eyes="open", mouth="neutral", tilt=0):
    """ox/oy = head box top-left in frame coords."""
    skin = lk.skin
    style = lk.get("style")
    hairr = lk.hair
    side = facing in ("left", "right")
    mask = SM if side else FM
    for (x, y) in mask:
        c = skin[2]
        rows = SIDE_HEAD if side else FRONT_HEAD
        if rows[y][x] == "s":
            c = skin[1]
        px(im, ox + x, oy + y, c)
    if facing == "down":
        # ears
        if style not in ("bob", "curly"):
            for y in (5, 6):
                px(im, ox - 1, oy + y, skin[1])
                px(im, ox + 12, oy + y, skin[1])
        draw_face_front(im, lk, ox, oy, eyes, mouth)
        hair = hair_front(style)
    elif facing == "up":
        for y in (5, 6):
            px(im, ox - 1, oy + y, skin[1])
            px(im, ox + 12, oy + y, skin[1])
        hair = hair_back(style)
    else:
        px(im, ox + 11, oy + 6, skin[2])  # nose
        px(im, ox + 11, oy + 7, skin[1])
        draw_face_side(im, lk, ox, oy, eyes, mouth)
        hair = hair_side(style)
        for y in (5, 6):
            px(im, ox + 4, oy + y, skin[1])
        px(im, ox + 5, oy + 5, skin[0])
    hs = shade_set(hair, hairr, 5)
    for (x, y), c in hs.items():
        px(im, ox + x, oy + y, c)
    if style == "part" and facing == "down":
        px(im, ox + 7, oy + 1, hairr[1])
        px(im, ox + 7, oy + 2, hairr[1])
    if style == "beanie":
        draw_beanie(im, lk, facing, ox, oy)
    if lk.get("beard") and facing != "up":
        draw_beard(im, lk, facing, ox, oy, mouth)
    if lk.get("glasses") and facing != "up":
        draw_glasses(im, facing, ox, oy, eyes)
    if lk.get("headset"):
        draw_headset(im, facing, ox, oy)


def eye_color():
    return C["ink"][1]


def draw_face_front(im, lk, ox, oy, eyes, mouth):
    e = eye_color()
    sk = lk.skin
    L, R = 3, 8
    ey = 5
    if eyes == "open":
        for x in (L, R):
            px(im, ox + x, oy + ey, e)
            px(im, ox + x, oy + ey + 1, e)
        px(im, ox + L - 1, oy + ey - 1, sk[1])
        px(im, ox + R + 1, oy + ey - 1, sk[1])
    elif eyes == "down":
        for x in (L, R):
            px(im, ox + x, oy + ey + 1, e)
            px(im, ox + x - 1, oy + ey, sk[1])
            px(im, ox + x, oy + ey, sk[1])
            px(im, ox + x + 1, oy + ey, sk[1])
    elif eyes in ("blink", "closed"):
        c = e if eyes == "blink" else sk[0]
        for x in (L, R):
            px(im, ox + x - (1 if x == L else 0), oy + ey + 1, c)
            px(im, ox + x + (0 if x == L else 1), oy + ey + 1, c)
    elif eyes == "happy":
        for x in (L, R):
            px(im, ox + x - 1, oy + ey + 1, e)
            px(im, ox + x, oy + ey, e)
            px(im, ox + x + 1, oy + ey + 1, e)
    elif eyes == "squeeze":
        for x, d in ((L, 1), (R, -1)):
            px(im, ox + x - d, oy + ey, e)
            px(im, ox + x, oy + ey + 1, e)
            px(im, ox + x + d, oy + ey + 1, e)
    elif eyes == "wide":
        for x in (L, R):
            px(im, ox + x, oy + ey - 1, e)
            px(im, ox + x, oy + ey, e)
            px(im, ox + x, oy + ey + 1, e)
    elif eyes == "side":
        for x in (L + 1, R + 1):
            px(im, ox + x, oy + ey, e)
            px(im, ox + x, oy + ey + 1, e)
    # cheeks
    blush = C["pink"][3] if lk.get("skin") in ("skin0", "skin1") else C["pink"][2]
    if eyes != "closed":
        px(im, ox + 2, oy + 7, blush)
        px(im, ox + 9, oy + 7, blush)
    if lk.get("freckles"):
        px(im, ox + 3, oy + 8, sk[1])
        px(im, ox + 8, oy + 8, sk[1])
    m = C["red"][1]
    my = 8
    if mouth == "neutral":
        px(im, ox + 5, oy + my, sk[0])
        px(im, ox + 6, oy + my, sk[0])
    elif mouth == "smile":
        px(im, ox + 4, oy + my - 1, sk[0])
        px(im, ox + 5, oy + my, m)
        px(im, ox + 6, oy + my, m)
        px(im, ox + 7, oy + my - 1, sk[0])
    elif mouth == "open":
        rect(im, ox + 5, oy + my, 2, 2, m)
        px(im, ox + 5, oy + my + 1, C["pink"][2])
    elif mouth == "grin":
        rect(im, ox + 4, oy + my, 4, 1, m)
        px(im, ox + 5, oy + my + 1, m)
        px(im, ox + 6, oy + my + 1, m)
        px(im, ox + 4, oy + my, WHITE_T)
        px(im, ox + 7, oy + my, WHITE_T)
    elif mouth == "frown":
        px(im, ox + 4, oy + my + 1, sk[0])
        px(im, ox + 5, oy + my, sk[0])
        px(im, ox + 6, oy + my, sk[0])
        px(im, ox + 7, oy + my + 1, sk[0])
    elif mouth == "o":
        px(im, ox + 5, oy + my, m)
        px(im, ox + 5, oy + my + 1, m)
    elif mouth == "flat":
        rect(im, ox + 4, oy + my, 4, 1, sk[0])


WHITE_T = hexc("#f6f2ea")


def draw_face_side(im, lk, ox, oy, eyes, mouth):
    e = eye_color()
    sk = lk.skin
    x = 8
    if eyes in ("open", "wide", "side"):
        px(im, ox + x, oy + 5, e)
        px(im, ox + x, oy + 6, e)
        px(im, ox + x - 1, oy + 4, sk[1])
    elif eyes == "down":
        px(im, ox + x, oy + 6, e)
        px(im, ox + x, oy + 5, sk[1])
    elif eyes in ("blink", "closed", "squeeze"):
        px(im, ox + x, oy + 6, e if eyes != "closed" else sk[0])
        px(im, ox + x - 1, oy + 6, e if eyes != "closed" else sk[0])
    elif eyes == "happy":
        px(im, ox + x - 1, oy + 6, e)
        px(im, ox + x, oy + 5, e)
        px(im, ox + x + 1, oy + 6, e)
    blush = C["pink"][3] if lk.get("skin") in ("skin0", "skin1") else C["pink"][2]
    if eyes != "closed":
        px(im, ox + 7, oy + 7, blush)
    m = C["red"][1]
    if mouth in ("open", "grin"):
        px(im, ox + 10, oy + 8, m)
        px(im, ox + 9, oy + 8, m)
        px(im, ox + 10, oy + 9, m)
    elif mouth == "o":
        px(im, ox + 10, oy + 8, m)
    elif mouth == "smile":
        px(im, ox + 9, oy + 8, m)
        px(im, ox + 10, oy + 7, sk[0])
    else:
        px(im, ox + 9, oy + 8, sk[0])
        px(im, ox + 10, oy + 8, sk[0])


def draw_beanie(im, lk, facing, ox, oy):
    hat = lk.get("hat")
    cells = set()
    if facing in ("down", "up"):
        cells |= span(-2, 3, 8) | span(-1, 1, 10) | span(0, 0, 11) | span(1, 0, 11) | span(2, -1, 12) | span(3, -1, 12)
        if facing == "up":
            cells |= span(4, -1, 12)
    else:
        cells |= span(-2, 2, 7) | span(-1, 1, 9) | span(0, 0, 10) | span(1, 0, 10) | span(2, -1, 10) | span(3, -1, 10)
    sh = shade_set(cells, [hat[0], hat[0], hat[1], hat[2]], 5)
    for (x, y), c in sh.items():
        band = y >= 2
        if band:
            c = hat[0] if (x % 2 == 0) else hat[1]
        px(im, ox + x, oy + y, c)
    px(im, ox + 5, oy - 3, C["cream"][4])
    px(im, ox + 6, oy - 3, C["cream"][3])


def draw_beard(im, lk, facing, ox, oy, mouth):
    hr = lk.hair
    if facing == "down":
        cells = span(8, 1, 3) | span(8, 8, 10) | span(9, 1, 10) | span(10, 3, 8) | {(1, 7), (10, 7)}
        cells |= span(7, 4, 7)  # moustache
        for (x, y) in cells:
            c = hr[2] if y < 10 else hr[1]
            if y == 7:
                c = hr[1]
            px(im, ox + x, oy + y, c)
        if mouth in ("open", "grin", "o"):
            px(im, ox + 5, oy + 8, C["red"][1])
            px(im, ox + 6, oy + 8, C["red"][1])
            px(im, ox + 5, oy + 9, C["red"][0])
            px(im, ox + 6, oy + 9, C["red"][0])
        else:
            px(im, ox + 5, oy + 8, hr[0])
            px(im, ox + 6, oy + 8, hr[0])
    else:
        cells = span(9, 3, 10) | span(10, 3, 8) | span(8, 4, 5) | span(8, 9, 10) | {(9, 7), (10, 7)}
        for (x, y) in cells:
            px(im, ox + x, oy + y, hr[2] if y < 10 else hr[1])


def draw_glasses(im, facing, ox, oy, eyes):
    f = C["ink"][2]
    g = C["sky"][3]
    if facing == "down":
        for x0 in (2, 7):
            for (x, y) in ((0, 0), (1, 0), (2, 0), (0, 1), (2, 1), (0, 2), (1, 2), (2, 2)):
                px(im, ox + x0 + x, oy + 4 + y, f)
        px(im, ox + 5, oy + 5, f)
        px(im, ox + 6, oy + 5, f)
        if eyes in ("open", "wide"):
            px(im, ox + 4, oy + 5, g)
            px(im, ox + 9, oy + 5, g)
    else:
        for (x, y) in ((7, 4), (8, 4), (9, 4), (7, 6), (8, 6), (9, 6), (9, 5), (6, 5), (5, 5), (4, 5)):
            px(im, ox + x, oy + y, f)


def draw_headset(im, facing, ox, oy):
    d = C["ink"][2]
    m = C["steel"][3]
    hi = C["steel"][5]
    led = C["teal"][4]
    if facing in ("down", "up"):
        for x in range(1, 11):
            px(im, ox + x, oy - 1, d)
        px(im, ox + 0, oy, d)
        px(im, ox + 11, oy, d)
        for y in range(1, 4):
            px(im, ox - 1, oy + y, d)
            px(im, ox + 12, oy + y, d)
        for xx in (-2, 12):
            rect(im, ox + xx, oy + 4, 2, 3, m)
            px(im, ox + xx, oy + 4, hi)
        if facing == "down":
            # boom mic to the mouth corner
            px(im, ox + 0, oy + 7, d)
            px(im, ox + 1, oy + 8, d)
            px(im, ox + 2, oy + 9, C["steel"][4])
    else:
        for x in range(1, 7):
            px(im, ox + x, oy - 1, d)
        for y in range(0, 4):
            px(im, ox + 4, oy + y, d)
        rect(im, ox + 3, oy + 4, 3, 3, m)
        px(im, ox + 3, oy + 4, hi)
        for i in range(4):
            px(im, ox + 6 + i, oy + 7 + (i // 2), d)
        px(im, ox + 10, oy + 9, led)


# ----------------------------------------------------------------- torso --

def draw_torso(im, lk, facing, oy, seated=False):
    """Torso top row at y = 15 + oy."""
    s, m, l = lk.cloth
    top = lk.get("top")
    y0 = 15 + oy
    if facing in ("down", "up"):
        rows = [(1, 8), (0, 9), (0, 9), (0, 9), (0, 9), (1, 8), (1, 8), (1, 8)]
        x0 = 7
        for j, (a, b) in enumerate(rows):
            for i in range(a, b + 1):
                c = m
                if i == a:
                    c = l
                elif i == b or j == 7:
                    c = s
                px(im, x0 + i, y0 + j, c)
        if facing == "down":
            torso_front_detail(im, lk, top, x0, y0)
        else:
            torso_back_detail(im, lk, top, x0, y0)
    else:
        rows = [(1, 6), (0, 7), (0, 7), (0, 7), (0, 7), (0, 7), (1, 7), (1, 7)]
        x0 = 8
        for j, (a, b) in enumerate(rows):
            for i in range(a, b + 1):
                c = m
                if i == a:
                    c = s
                elif i == b and j < 6:
                    c = l
                if j == 7:
                    c = s
                px(im, x0 + i, y0 + j, c)
        torso_side_detail(im, lk, top, x0, y0)


def torso_front_detail(im, lk, top, x0, y0):
    s, m, l = lk.cloth
    sk = lk.skin
    acc = lk.accent
    if top == "blazer":
        sh = lk.shirt
        for j, (a, b) in enumerate([(3, 6), (3, 6), (4, 5), (4, 5), (4, 5)]):
            for i in range(a, b + 1):
                px(im, x0 + i, y0 + j, sh)
        for j in range(0, 5):
            px(im, x0 + 4 + (1 if j > 0 else 0), y0 + j, acc) if j > 0 else None
        px(im, x0 + 4, y0 + 1, acc)
        px(im, x0 + 5, y0 + 1, acc)
        px(im, x0 + 4, y0 + 2, acc)
        px(im, x0 + 5, y0 + 2, C["red"][2])
        px(im, x0 + 4, y0 + 3, acc)
        px(im, x0 + 5, y0 + 3, C["red"][2])
        px(im, x0 + 4, y0 + 4, C["red"][2])
        px(im, x0 + 5, y0 + 4, C["red"][2])
        for j in range(1, 6):  # lapel edges
            px(im, x0 + 3 - (0 if j < 3 else 0), y0 + j, l if j < 4 else m)
            px(im, x0 + 6, y0 + j, s if j < 4 else m)
        px(im, x0 + 2, y0 + 4, C["steel"][6])  # pocket square
        px(im, x0 + 7, y0 + 6, C["yellow"][3])  # button
        px(im, x0 + 4, y0 + 0, sh)
        px(im, x0 + 5, y0 + 0, sh)
    elif top == "hoodie":
        for i in range(2, 8):
            px(im, x0 + i, y0, s)
        px(im, x0 + 4, y0 + 1, sk[1])
        px(im, x0 + 5, y0 + 1, sk[1])
        px(im, x0 + 3, y0 + 1, s)
        px(im, x0 + 6, y0 + 1, s)
        px(im, x0 + 3, y0 + 2, acc)
        px(im, x0 + 3, y0 + 3, acc)
        px(im, x0 + 6, y0 + 2, acc)
        px(im, x0 + 6, y0 + 3, acc)
        for i in range(2, 8):
            px(im, x0 + i, y0 + 5, s)
        px(im, x0 + 2, y0 + 6, s)
        px(im, x0 + 7, y0 + 6, s)
    elif top == "sweater":
        px(im, x0 + 3, y0, acc)
        px(im, x0 + 6, y0, acc)
        px(im, x0 + 4, y0, sk[1])
        px(im, x0 + 5, y0, sk[1])
        for i in range(1, 9):
            px(im, x0 + i, y0 + 7, s if i % 2 else m)
        # cable knit
        for j in range(2, 6):
            px(im, x0 + 4 + (j % 2), y0 + j, l)
    elif top == "shirt":
        px(im, x0 + 4, y0, sk[2])
        px(im, x0 + 5, y0, sk[2])
        px(im, x0 + 3, y0, l)
        px(im, x0 + 6, y0, l)
        px(im, x0 + 4, y0 + 1, sk[1])
        px(im, x0 + 5, y0 + 1, s)
        for j in range(2, 7, 2):
            px(im, x0 + 5, y0 + j, acc)
        px(im, x0 + 2, y0 + 2, s)
        px(im, x0 + 3, y0 + 2, s)
    elif top == "flannel":
        for j in range(0, 8):
            for i in range(0, 10):
                if (i // 2 + j // 2) % 2 == 0 and im.getpixel((x0 + i, y0 + j))[3]:
                    c = im.getpixel((x0 + i, y0 + j))
                    if c == m:
                        px(im, x0 + i, y0 + j, s)
                    elif c == l:
                        px(im, x0 + i, y0 + j, m)
            px(im, x0 + 5, y0 + j, acc if j > 0 else sk[1])
        px(im, x0 + 4, y0, sk[1])
        px(im, x0 + 4, y0 + 1, C["steel"][6])
    elif top == "tee":
        px(im, x0 + 4, y0, sk[1])
        px(im, x0 + 5, y0, sk[1])
        px(im, x0 + 3, y0, s)
        px(im, x0 + 6, y0, s)
        # small chest print
        if lk.get("cloth") == "teal":
            px(im, x0 + 4, y0 + 3, acc)
            px(im, x0 + 5, y0 + 3, acc)
            px(im, x0 + 4, y0 + 4, acc)
            px(im, x0 + 5, y0 + 2, acc)
        else:
            px(im, x0 + 3, y0 + 3, C["cream"][4])
            px(im, x0 + 4, y0 + 4, C["cream"][4])
            px(im, x0 + 5, y0 + 3, C["cream"][4])
        if lk.get("phones"):
            ph = C["ink"][2]
            for i in range(1, 9):
                px(im, x0 + i, y0 - 1 + (0 if 2 < i < 7 else 1), ph)
            rect(im, x0 + 0, y0 + 0, 2, 3, ph)
            rect(im, x0 + 8, y0 + 0, 2, 3, ph)
            px(im, x0 + 0, y0 + 1, C["pink"][3])
            px(im, x0 + 9, y0 + 1, C["pink"][3])
    elif top == "cardigan":
        sh = lk.shirt
        for j in range(0, 8):
            px(im, x0 + 4, y0 + j, sh)
            px(im, x0 + 5, y0 + j, sh)
        px(im, x0 + 4, y0, sk[1])
        px(im, x0 + 5, y0, sk[1])
        for j in (2, 4, 6):
            px(im, x0 + 3, y0 + j, acc)
        px(im, x0 + 1, y0 + 5, s)
        px(im, x0 + 2, y0 + 5, s)
        px(im, x0 + 8, y0 + 5, s)


def torso_back_detail(im, lk, top, x0, y0):
    s, m, l = lk.cloth
    if top == "hoodie":
        rect(im, x0 + 2, y0, 6, 3, s)
        rect(im, x0 + 3, y0, 4, 2, m)
        px(im, x0 + 3, y0 + 2, s)
    elif top == "blazer":
        px(im, x0 + 4, y0, lk.shirt)
        px(im, x0 + 5, y0, lk.shirt)
        for j in range(3, 8):
            px(im, x0 + 5, y0 + j, s)
    elif top == "flannel":
        for j in range(0, 8):
            for i in range(0, 10):
                c = im.getpixel((x0 + i, y0 + j))
                if (i // 2 + j // 2) % 2 == 0 and c == m:
                    px(im, x0 + i, y0 + j, s)
    elif top == "sweater":
        for i in range(1, 9):
            px(im, x0 + i, y0 + 7, s if i % 2 else m)
    elif top == "cardigan":
        for i in range(1, 9):
            px(im, x0 + i, y0 + 6, s)
    if lk.get("phones"):
        ph = C["ink"][2]
        for i in range(1, 9):
            px(im, x0 + i, y0, ph)


def torso_side_detail(im, lk, top, x0, y0):
    s, m, l = lk.cloth
    sk = lk.skin
    if top == "blazer":
        px(im, x0 + 6, y0, lk.shirt)
        px(im, x0 + 7, y0, lk.shirt)
        px(im, x0 + 7, y0 + 1, lk.shirt)
        px(im, x0 + 7, y0 + 2, lk.accent)
        px(im, x0 + 7, y0 + 3, lk.accent)
        px(im, x0 + 6, y0 + 1, l)
    elif top == "hoodie":
        rect(im, x0 + 0, y0, 3, 3, s)
        px(im, x0 + 7, y0 + 2, lk.accent)
        px(im, x0 + 7, y0 + 3, lk.accent)
    elif top in ("tee", "shirt", "sweater"):
        px(im, x0 + 6, y0, sk[1])
        px(im, x0 + 7, y0, sk[1])
    elif top == "cardigan":
        rect(im, x0 + 6, y0, 2, 8, lk.shirt)
        px(im, x0 + 6, y0, sk[1])
    elif top == "flannel":
        for j in range(0, 8):
            for i in range(0, 8):
                c = im.getpixel((x0 + i, y0 + j))
                if (i // 2 + j // 2) % 2 == 0 and c == m:
                    px(im, x0 + i, y0 + j, s)
        px(im, x0 + 7, y0, sk[1])
    if lk.get("phones"):
        rect(im, x0 + 3, y0 - 1, 3, 2, C["ink"][2])
        px(im, x0 + 4, y0 - 1, C["pink"][3])


# ------------------------------------------------------------------ limbs --

def brush(im, x, y, w, h, c):
    rect(im, x, y, w, h, c)


def seg_points(a, b):
    (x0, y0), (x1, y1) = a, b
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    return [(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n)) for i in range(n + 1)]


def draw_arm(im, lk, pts, side, sleeve="long", base=None):
    """pts absolute; side 'L' draws the brush to the left of the line."""
    s, m, l = lk.cloth
    if lk.get("top") in ("blazer",):
        pass
    col = l if side == "L" else m
    skin = lk.skin
    short = lk.get("top") == "tee"
    allp = []
    for i in range(len(pts) - 1):
        sp = seg_points(pts[i], pts[i + 1])
        allp.extend(sp if i == 0 else sp[1:])
    n = len(allp)
    for k, (x, y) in enumerate(allp):
        c = col
        if short and k >= 3:
            c = skin[2] if side == "L" else skin[1]
        bx = x - 1 if side == "L" else x
        rect(im, bx, y, 2, 1, c)
        # cuff: darker ring just before the hand
        if not short and k == n - 2 and n > 3:
            rect(im, bx, y, 2, 1, s)
    hx, hy = allp[-1]
    bx = hx - 1 if side == "L" else hx
    rect(im, bx, hy, 2, 2, skin[2])
    px(im, bx + (1 if side == "L" else 0), hy + 1, skin[1])


def draw_legs(im, lk, facing, mode, ly=0):
    p = lk.pants
    sh = lk.shoes
    shd = C["ink"][1]
    if mode == "hidden":
        return
    if facing in ("down", "up"):
        base = 23 + ly
        if mode.startswith("walk"):
            ph = int(mode[-1])
            lift = {0: (0, 1), 1: (0, 0), 2: (1, 0), 3: (0, 0)}[ph]
        elif mode == "jump":
            lift = (1, 1)
        else:
            lift = (0, 0)
        for leg, x0 in ((0, 8), (1, 12)):
            up = lift[leg]
            h = 6 - up
            for j in range(h):
                for i in range(4):
                    c = p[1]
                    if leg == 0 and i == 0:
                        c = p[2]
                    if leg == 1 and i == 3:
                        c = p[0]
                    if j == 0:
                        c = p[1] if c != p[0] else p[0]
                    px(im, x0 + i, base + j, c)
            if facing == "down" and leg == 1:
                for j in range(2, h):
                    px(im, 12, base + j, p[0])
            fy = base + h
            rect(im, x0, fy, 4, 2, sh)
            hl = C["steel"][6] if sh != C["steel"][6] else C["steel"][7]
            if facing == "down":
                px(im, x0 + (0 if leg == 0 else 3), fy + 1, shd if sh == C["steel"][6] else sh)
                px(im, x0 + 1, fy, hl if sh in (C["steel"][6],) else C["ink"][3])
    elif facing in ("left", "right"):
        base = 23 + ly
        hip = (11, base)
        if mode.startswith("walk"):
            ph = int(mode[-1])
            spread = {0: 2, 1: 0, 2: -2, 3: 0}[ph]
        else:
            spread = 0
        far = (11 - spread, base + 6)
        near = (11 + spread, base + 6)
        if mode.startswith("walk") and spread == 0:
            near = (12, base + 5)
        for which, foot, col in (("far", far, p[0]), ("near", near, p[1])):
            for (x, y) in seg_points(hip, (foot[0], foot[1] - 1)):
                rect(im, x - 1, y, 3, 1, col)
            px(im, foot[0] + 1, foot[1] - 1, p[2] if which == "near" else p[0])
            rect(im, foot[0] - 1, foot[1], 4, 2, sh)
            if which == "far":
                rect(im, foot[0] - 1, foot[1] + 1, 4, 1, C["ink"][1] if sh != C["ink"][1] else C["ink"][0])


def draw_lap_front(im, lk, ly=0):
    """Seated, facing the camera: knees toward us, shins down to the floor."""
    p = lk.pants
    sh = lk.shoes
    base = 23 + ly
    rect(im, 7, base, 10, 3, p[1])
    hline(im, 7, base, 10, p[2])
    px(im, 11, base + 1, p[0])
    px(im, 12, base + 1, p[0])
    px(im, 16, base + 1, p[0])
    px(im, 16, base + 2, p[0])
    for x0 in (8, 12):
        rect(im, x0, base + 3, 4, 31 - 2 - (base + 3), p[0])
        px(im, x0, base + 3, p[1])
        rect(im, x0, 29, 4, 2, sh)


def hline(im, x, y, w, c):
    rect(im, x, y, w, 1, c)


def draw_lap_side(im, lk, ly=0):
    """Seated, facing right: thigh forward, shin down."""
    p = lk.pants
    sh = lk.shoes
    base = 23 + ly
    rect(im, 9, base, 9, 3, p[1])
    hline(im, 9, base, 9, p[2])
    rect(im, 15, base + 3, 3, 31 - 2 - (base + 3), p[0])
    rect(im, 15, 29, 5, 2, sh)


# ----------------------------------------------------------------- puppet --

SH_L = (6, 16)
SH_R = (17, 16)


def arm_preset(name, side, dy):
    """Arm polylines relative to the shoulder; written for the left arm."""
    P = {
        "hang": [(0, 0), (0, 3), (0, 6)],
        "hang_in": [(0, 0), (0, 3), (1, 6)],
        "swing_f": [(0, 0), (0, 3), (1, 5)],
        "swing_b": [(0, 0), (-1, 3), (-1, 6)],
        "up": [(0, 0), (-1, -4), (-1, -9)],
        "up_wave_a": [(0, 0), (-2, -3), (-4, -8)],
        "up_wave_b": [(0, 0), (-1, -4), (0, -9)],
        "v": [(0, 0), (-2, -4), (-3, -8)],
        "face": [(0, 0), (-2, -3), (4, -8)],
        "head": [(0, 0), (-2, -3), (-1, -7)],
        "chin": [(0, 0), (1, 4), (3, 1), (4, -3)],
        "type_a": [(0, 0), (-1, 3), (0, 5), (3, 6)],
        "type_b": [(0, 0), (-1, 3), (0, 4), (3, 5)],
        "desk": [(0, 0), (0, 3), (2, 6), (4, 6)],
        "lap": [(0, 0), (0, 3), (2, 7), (4, 7)],
        "gesture_a": [(0, 0), (-1, 3), (-4, 3)],
        "gesture_b": [(0, 0), (-1, 3), (-4, 1)],
        "mug": [(0, 0), (0, 4), (3, 5)],
        "mug_up": [(0, 0), (1, 4), (4, -1)],
        "hip": [(0, 0), (-1, 3), (1, 5)],
        "stretch": [(0, 0), (-1, -6), (1, -12)],
        "back_hold": [(0, 0), (-1, 3), (0, 6), (2, 6)],
    }
    pts = P[name]
    sx, sy = SH_L if side == "L" else SH_R
    sgn = 1 if side == "L" else -1
    return [(sx + sgn * x, sy + dy + y) for (x, y) in pts]


def render(lk, pose):
    f = pose.get("facing", "down")
    if f == "left":
        q = dict(pose)
        q["facing"] = "right"
        return mirror(render(lk, q))
    im = new(FW, FH)
    dy = pose.get("dy", 0)
    ly = pose.get("ly", 0)
    legs = pose.get("legs", "stand")
    eyes = pose.get("eyes", "open")
    mouth = pose.get("mouth", "neutral")
    hdx, hdy = pose.get("hdx", 0), pose.get("hdy", 0)
    arms = pose.get("arms", ("hang", "hang"))
    behind = pose.get("behind", False)
    item = pose.get("item")
    body = new(FW, FH)

    if f in ("down", "up"):
        if f == "up" and lk.get("style") == "ponytail":
            pass
        # legs
        if legs == "lap":
            draw_lap_front(body, lk, ly)
        else:
            draw_legs(body, lk, f, legs, ly)
        arm_l = arm_preset(arms[0], "L", dy) if isinstance(arms[0], str) else arms[0]
        arm_r = arm_preset(arms[1], "R", dy) if isinstance(arms[1], str) else arms[1]
        if behind:
            draw_arm(body, lk, arm_l, "L")
            draw_arm(body, lk, arm_r, "R")
        draw_torso(body, lk, f, dy)
        up_l = arms[0] in ("up", "up_wave_a", "up_wave_b", "v", "head", "stretch")
        up_r = arms[1] in ("up", "up_wave_a", "up_wave_b", "v", "head", "stretch")
        front_l = arms[0] in ("face", "chin", "mug_up")
        front_r = arms[1] in ("face", "chin", "mug_up")
        arm_layer = new(FW, FH)
        if not behind:
            if not front_l and not up_l:
                draw_arm(arm_layer, lk, arm_l, "L")
            if not front_r and not up_r:
                draw_arm(arm_layer, lk, arm_r, "R")
        inner_outline(body, arm_layer, lk.cloth[0])
        hx = 6 + hdx
        hy = 4 + dy + hdy
        draw_head(body, lk, f, hx, hy, eyes, mouth)
        if f == "up" and lk.get("style") == "ponytail":
            pass
        top = new(FW, FH)
        for a, side, flag in ((arm_l, "L", up_l or front_l), (arm_r, "R", up_r or front_r)):
            if flag and not behind:
                draw_arm(top, lk, a, side)
        inner_outline(body, top, lk.skin[0] if f == "down" else lk.cloth[0])
        if item:
            draw_item(body, item, dy)
    else:
        # facing right
        if legs == "lap":
            draw_lap_side(body, lk, ly)
        else:
            draw_legs(body, lk, f, legs, ly)
        far = pose.get("far")
        near = pose.get("near", [(12, 16 + dy), (12, 19 + dy), (12, 22 + dy)])
        if far:
            draw_arm(body, lk, [(x, y + dy) for x, y in far], "R")
        draw_torso(body, lk, f, dy)
        hx = 7 + hdx
        hy = 4 + dy + hdy
        near_over_head = pose.get("near_top", False)
        if not near_over_head:
            al = new(FW, FH)
            draw_arm(al, lk, [(x, y + (dy if pose.get("near_rel", True) else 0)) for x, y in near], "R")
            inner_outline(body, al, lk.cloth[0])
        draw_head(body, lk, f, hx, hy, eyes, mouth)
        if near_over_head:
            al = new(FW, FH)
            draw_arm(al, lk, [(x, y + dy) for x, y in near], "R")
            inner_outline(body, al, lk.skin[0])
        if item:
            draw_item(body, item, dy)
    outline(body, OUTLINE)
    im.alpha_composite(body)
    return im


def inner_outline(body, layer, col):
    """Composite `layer` over `body`, outlining it where it covers body pixels."""
    W, H = body.size
    lp = layer.load()
    bp = body.load()
    edge = []
    for y in range(H):
        for x in range(W):
            if lp[x, y][3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < W and 0 <= yy < H and lp[xx, yy][3] and bp[x, y][3]:
                    edge.append((x, y))
                    break
    body.alpha_composite(layer)
    for (x, y) in edge:
        bp[x, y] = col


def draw_item(im, item, dy):
    kind, x, y = item[0], item[1], item[2] + dy
    if kind == "mug":
        col = item[3] if len(item) > 3 else C["cream"][4]
        rect(im, x, y, 3, 3, col)
        px(im, x, y, C["red"][2])
        px(im, x + 1, y, C["red"][2])
        px(im, x + 2, y, C["red"][2])
        px(im, x + 3, y + 1, col)
        px(im, x + 1, y, C["wood"][1])
    elif kind == "papers":
        rect(im, x, y, 5, 4, C["cream"][4])
        hline(im, x + 1, y + 1, 3, C["steel"][4])
        hline(im, x + 1, y + 2, 2, C["steel"][4])
    elif kind == "box":
        rect(im, x, y, 8, 6, C["oak"][3])
        hline(im, x, y, 8, C["oak"][4])
        hline(im, x, y + 2, 8, C["oak"][2])


# ------------------------------------------------------------- animations --

def D(**k):
    k.setdefault("facing", "down")
    return k


def U(**k):
    k.setdefault("facing", "up")
    return k


def R(**k):
    k.setdefault("facing", "right")
    return k


def anims():
    """name -> (poses, frame sequence as pose indices, fps, loop)."""
    A = {}
    # idle standing
    A["idle_down"] = ([D(), D(dy=1), D(eyes="blink")], [0, 0, 0, 1, 1, 1, 0, 0, 2, 0, 1, 1], 4, True)
    A["idle_up"] = ([U(), U(dy=1)], [0, 0, 0, 1, 1, 1], 4, True)
    nr = [(12, 16), (12, 19), (13, 22)]
    A["idle_right"] = ([R(near=nr), R(near=nr, dy=1), R(near=nr, eyes="blink")], [0, 0, 0, 1, 1, 1, 0, 0, 2, 0, 1, 1], 4, True)
    # walking
    wd = []
    for i, (al, ar, bob) in enumerate((("swing_f", "swing_b", 0), ("hang", "hang", -1), ("swing_b", "swing_f", 0), ("hang", "hang", -1))):
        wd.append(D(legs="walk%d" % i, arms=(al, ar), dy=bob))
    A["walk_down"] = (wd, [0, 1, 2, 3], 8, True)
    wu = []
    for i, (al, ar, bob) in enumerate((("swing_b", "swing_f", 0), ("hang", "hang", -1), ("swing_f", "swing_b", 0), ("hang", "hang", -1))):
        wu.append(U(legs="walk%d" % i, arms=(al, ar), dy=bob))
    A["walk_up"] = (wu, [0, 1, 2, 3], 8, True)
    wr = []
    nears = ([(12, 16), (11, 19), (10, 22)], [(12, 16), (12, 19), (13, 22)], [(12, 16), (13, 19), (15, 21)], [(12, 16), (12, 19), (13, 22)])
    fars = ([(10, 16), (11, 19), (13, 21)], None, [(10, 16), (9, 19), (8, 21)], None)
    for i in range(4):
        wr.append(R(legs="walk%d" % i, near=nears[i], far=fars[i], dy=-1 if i % 2 else 0))
    A["walk_right"] = (wr, [0, 1, 2, 3], 8, True)
    # gestures, standing, facing the camera
    A["wave"] = ([D(arms=("hang", "up_wave_a"), mouth="smile"), D(arms=("hang", "up_wave_b"), mouth="open", eyes="happy")],
                 [0, 1, 0, 1, 0, 1], 6, True)
    A["cheer"] = ([D(arms=("v", "v"), mouth="grin", eyes="happy", dy=-2, ly=-2, legs="jump"),
                   D(arms=("v", "v"), mouth="grin", eyes="happy", dy=0),
                   D(arms=("up", "up"), mouth="open", eyes="happy", dy=-3, ly=-3, legs="jump"),
                   D(arms=("hang", "hang"), mouth="smile", eyes="happy", dy=1)],
                  [0, 2, 0, 1, 3], 7, True)
    A["facepalm"] = ([D(arms=("hang", "hang"), eyes="wide", mouth="o"),
                      D(arms=("face", "hang"), eyes="squeeze", mouth="frown", hdy=1),
                      D(arms=("face", "hang"), eyes="squeeze", mouth="frown", hdy=1, dy=1)],
                     [0, 1, 2, 1, 2, 1], 5, False)
    A["question"] = ([D(arms=("hang", "up"), mouth="o"), D(arms=("hang", "up"), mouth="o", dy=-1), D(arms=("hang", "up"), eyes="blink", mouth="o")],
                     [0, 1, 0, 1, 2, 1], 4, True)
    A["question_up"] = ([U(arms=("hang", "up")), U(arms=("hang", "up"), dy=-1)], [0, 1], 3, True)
    A["talk_down"] = ([D(arms=("gesture_a", "hang"), mouth="open"), D(arms=("gesture_b", "hang"), mouth="neutral"),
                       D(arms=("hang", "gesture_a"), mouth="open", eyes="blink"), D(arms=("hip", "gesture_b"), mouth="smile")],
                      [0, 1, 0, 3, 2, 1], 5, True)
    A["talk_up"] = ([U(arms=("gesture_a", "hang")), U(arms=("gesture_b", "hang"), dy=1),
                     U(arms=("hang", "gesture_a")), U(arms=("hang", "gesture_b"), dy=1)], [0, 1, 0, 1, 2, 3, 2, 3], 5, True)
    tr = [(12, 16), (14, 18), (17, 17)]
    tr2 = [(12, 16), (14, 18), (17, 15)]
    A["talk_right"] = ([R(near=tr, mouth="open"), R(near=tr2, mouth="neutral"), R(near=nr, mouth="open"), R(near=nr, mouth="smile", eyes="blink")],
                       [0, 1, 0, 1, 2, 3], 5, True)
    A["drink"] = ([D(arms=("hang", "mug"), item=("mug", 15, 21)),
                   D(arms=("hang", "mug_up"), item=("mug", 13, 13), eyes="closed", mouth="o"),
                   D(arms=("hang", "mug"), item=("mug", 15, 21), eyes="blink", mouth="smile")],
                  [0, 0, 0, 1, 1, 1, 2, 0], 4, True)
    A["stretch"] = ([D(arms=("stretch", "stretch"), eyes="closed", mouth="o", dy=-1),
                     D(arms=("up", "up"), eyes="closed", mouth="open", dy=-1),
                     D(arms=("hang", "hang"), eyes="happy", mouth="smile")],
                    [0, 1, 1, 0, 2], 3, False)
    A["carry_down"] = ([D(legs="walk%d" % i, arms=("back_hold", "back_hold"), dy=-1 if i % 2 else 0, item=("box", 8, 17)) for i in range(4)],
                       [0, 1, 2, 3], 8, True)
    # seated at a desk, seen from behind (worker faces the screen, up)
    S = dict(legs="hidden", behind=True)
    A["sit_type_up"] = ([U(arms=("type_a", "type_b"), dy=1, **S), U(arms=("type_b", "type_a"), dy=1, **S),
                         U(arms=("type_a", "type_a"), dy=1, hdy=0, **S), U(arms=("type_b", "type_b"), dy=1, hdx=0, **S)],
                        [0, 1, 0, 1, 2, 1, 0, 3], 8, True)
    A["sit_idle_up"] = ([U(arms=("desk", "desk"), dy=1, **S), U(arms=("desk", "desk"), dy=2, **S)], [0, 0, 1, 1], 3, True)
    A["sit_read_up"] = ([U(arms=("desk", "type_a"), dy=1, hdy=1, **S), U(arms=("desk", "type_b"), dy=1, hdy=1, **S),
                         U(arms=("desk", "type_a"), dy=2, hdy=1, **S)], [0, 0, 0, 1, 0, 0, 2, 2], 3, True)
    A["sit_think_up"] = ([U(arms=("desk", "head"), dy=1, hdx=1, behind=False, legs="hidden"),
                          U(arms=("desk", "head"), dy=1, hdx=1, hdy=-1, behind=False, legs="hidden"),
                          U(arms=("desk", "desk"), dy=1, hdx=-1, **S)],
                         [0, 0, 1, 0, 1, 0, 2, 2, 2], 3, True)
    A["sit_cheer_up"] = ([U(arms=("v", "v"), dy=0, behind=False, legs="hidden"), U(arms=("up", "up"), dy=-1, behind=False, legs="hidden")], [0, 1], 6, True)
    A["sit_facepalm_up"] = ([U(arms=("head", "head"), dy=2, hdy=1, behind=False, legs="hidden"),
                             U(arms=("head", "head"), dy=2, hdy=2, behind=False, legs="hidden")], [0, 1, 0, 1], 3, True)
    A["sit_raise_up"] = ([U(arms=("desk", "up"), dy=1, behind=False, legs="hidden"), U(arms=("desk", "up"), dy=0, behind=False, legs="hidden")], [0, 1], 3, True)
    # seated at a desk, facing the camera (monitor back toward us)
    F = dict(legs="hidden")
    A["sit_type_down"] = ([D(arms=("desk", "desk"), eyes="down", dy=1, **F), D(arms=("desk", "desk"), eyes="down", dy=2, **F),
                           D(arms=("desk", "desk"), eyes="open", dy=1, mouth="flat", **F), D(arms=("desk", "desk"), eyes="down", dy=1, hdx=0, mouth="o", **F)],
                          [0, 1, 0, 1, 0, 1, 2, 0, 1, 3], 8, True)
    A["sit_idle_down"] = ([D(arms=("desk", "desk"), dy=1, **F), D(arms=("desk", "desk"), dy=2, **F), D(arms=("desk", "desk"), dy=1, eyes="blink", **F)],
                          [0, 0, 1, 1, 0, 0, 2, 1], 3, True)
    A["sit_read_down"] = ([D(arms=("desk", "desk"), eyes="down", dy=1, hdy=1, **F), D(arms=("desk", "desk"), eyes="side", dy=1, hdy=1, **F),
                           D(arms=("desk", "desk"), eyes="blink", dy=1, hdy=1, **F)], [0, 0, 1, 1, 0, 2], 3, True)
    A["sit_think_down"] = ([D(arms=("desk", "chin"), eyes="side", dy=1, hdx=1, **F), D(arms=("desk", "chin"), eyes="side", mouth="flat", dy=1, hdx=1, **F),
                            D(arms=("desk", "head"), eyes="open", mouth="o", dy=1, **F)], [0, 0, 1, 1, 0, 2, 2], 3, True)
    A["sit_cheer_down"] = ([D(arms=("v", "v"), eyes="happy", mouth="grin", dy=0, **F), D(arms=("up", "up"), eyes="happy", mouth="open", dy=-1, **F)], [0, 1], 6, True)
    A["sit_facepalm_down"] = ([D(arms=("face", "desk"), eyes="squeeze", mouth="frown", dy=2, hdy=1, **F),
                               D(arms=("face", "face"), eyes="squeeze", mouth="frown", dy=2, hdy=1, **F)], [0, 1, 1, 0], 3, True)
    A["sit_raise_down"] = ([D(arms=("desk", "up"), mouth="o", dy=1, **F), D(arms=("desk", "up"), mouth="o", dy=0, **F)], [0, 1], 3, True)
    A["sit_talk_down"] = ([D(arms=("desk", "gesture_a"), mouth="open", dy=1, **F), D(arms=("desk", "gesture_b"), mouth="neutral", dy=1, **F),
                           D(arms=("desk", "desk"), mouth="smile", dy=1, eyes="blink", **F)], [0, 1, 0, 1, 2, 2], 5, True)
    # lounge: facing the camera
    Lp = dict(legs="lap")
    A["lounge_relax_down"] = ([D(arms=("lap", "lap"), dy=2, ly=0, mouth="smile", **Lp), D(arms=("lap", "lap"), dy=3, mouth="smile", **Lp),
                               D(arms=("lap", "lap"), dy=2, eyes="blink", mouth="smile", **Lp)], [0, 0, 1, 1, 0, 2, 1], 3, True)
    A["lounge_sip_down"] = ([D(arms=("lap", "mug"), item=("mug", 15, 21), dy=2, **Lp),
                             D(arms=("lap", "mug_up"), item=("mug", 13, 13), eyes="closed", mouth="o", dy=2, **Lp),
                             D(arms=("lap", "mug"), item=("mug", 15, 21), eyes="happy", mouth="smile", dy=2, **Lp)],
                            [0, 0, 0, 1, 1, 1, 2, 2, 0], 3, True)
    A["lounge_sleep_down"] = ([D(arms=("lap", "lap"), dy=3, hdx=1, hdy=1, eyes="closed", mouth="o", **Lp),
                               D(arms=("lap", "lap"), dy=3, hdx=1, hdy=2, eyes="closed", mouth="neutral", **Lp)], [0, 0, 1, 1], 2, True)
    # lounge: facing right (mirror for left)
    lap_near = [(12, 18), (13, 21), (16, 22)]
    A["lounge_relax_right"] = ([R(legs="lap", dy=2, near=lap_near, mouth="smile"), R(legs="lap", dy=3, near=lap_near, mouth="smile"),
                                R(legs="lap", dy=2, near=lap_near, eyes="blink")], [0, 0, 1, 1, 0, 2, 1], 3, True)
    A["lounge_sip_right"] = ([R(legs="lap", dy=2, near=[(12, 16), (13, 20), (16, 19)], item=("mug", 16, 17)),
                              R(legs="lap", dy=2, near=[(12, 16), (14, 19), (17, 13)], item=("mug", 17, 11), eyes="closed", near_top=True),
                              R(legs="lap", dy=2, near=[(12, 16), (13, 20), (16, 19)], item=("mug", 16, 17), eyes="happy", mouth="smile")],
                             [0, 0, 0, 1, 1, 1, 2, 2, 0], 3, True)
    A["lounge_sleep_right"] = ([R(legs="lap", dy=3, hdx=-1, hdy=1, near=lap_near, eyes="closed", mouth="o"),
                                R(legs="lap", dy=3, hdx=-1, hdy=2, near=lap_near, eyes="closed")], [0, 0, 1, 1], 2, True)
    return A


MIRROR = {"idle_right": "idle_left", "walk_right": "walk_left", "talk_right": "talk_left",
          "lounge_relax_right": "lounge_relax_left", "lounge_sip_right": "lounge_sip_left",
          "lounge_sleep_right": "lounge_sleep_left"}


def build():
    A = anims()
    looks = [Look(d) for d in LOOKS]
    order = []  # (anim name, poses, mirrored)
    for name, (poses, seq, fps, loop) in A.items():
        order.append((name, poses, seq, fps, loop, False))
        if name in MIRROR:
            order.append((MIRROR[name], poses, seq, fps, loop, True))
    total = sum(len(o[1]) for o in order)
    sheet = Sheet("characters", FW, FH, cols=total, anchor=ANCHOR, kind="character")
    sheet.extra["variants"] = {"key": "look", "count": len(looks),
                               "rule": "row = look; frame index = column"}
    frames = []  # per look list of frames in column order
    col = 0
    for name, poses, seq, fps, loop, mir in order:
        base = col
        col += len(poses)
        sheet.anim(name, [base + i for i in seq], fps, loop)
    for lk in looks:
        for name, poses, seq, fps, loop, mir in order:
            for p in poses:
                if mir:
                    p = dict(p)
                    p["facing"] = "left"
                sheet.add(render(lk, p))
    sheet.save()
    return sheet, order
