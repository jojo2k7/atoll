# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""In-scene UI kit: 9-slice panels, controls, bubbles, emotes, icons, gauges,
and the system room (a PC case cut-away hosting the load meters)."""

import math

from .core import (C, T, OUTLINE, WHITE, hexc, alpha, new, px, rect, hline, vline,
                   frame_rect, line, ellipse, stamp, outline, Sheet, single)

INK = C["ink"]
ST = C["steel"]
ACC = C["orange"]
UI_HI = hexc("#6e5c78")
UI_BG = hexc("#251c2c")
UI_BG2 = hexc("#2f2438")
UI_LINE = hexc("#4a3b54")
TEXT = hexc("#f2e8dc")
TEXT_DIM = hexc("#9c8ea6")


def rounded(im, x, y, w, h, c):
    rect(im, x + 1, y, w - 2, h, c)
    rect(im, x, y + 1, w, h - 2, c)


def panel_dark():
    s = 24
    im = new(s, s)
    rounded(im, 0, 0, s, s, INK[0])
    rounded(im, 1, 1, s - 2, s - 2, UI_LINE)
    rect(im, 2, 2, s - 4, s - 4, UI_BG)
    hline(im, 2, 2, s - 4, UI_HI)
    vline(im, 2, 3, s - 6, alpha(UI_HI, 140))
    hline(im, 2, s - 3, s - 4, hexc("#1a1320"))
    # inner subtle top band
    rect(im, 3, 3, s - 6, 2, UI_BG2)
    return im


def panel_light():
    s = 24
    im = new(s, s)
    cr = C["cream"]
    rounded(im, 0, 0, s, s, INK[1])
    rounded(im, 1, 1, s - 2, s - 2, cr[1])
    rect(im, 2, 2, s - 4, s - 4, cr[4])
    hline(im, 2, 2, s - 4, WHITE)
    hline(im, 2, s - 3, s - 4, cr[2])
    vline(im, s - 3, 3, s - 6, cr[3])
    return im


def button(state):
    w, h = 16, 14
    im = new(w, h)
    off = 1 if state == 2 else 0
    face = {0: hexc("#4a3b54"), 1: hexc("#5c4a68"), 2: hexc("#3a2e44")}[state]
    hi = {0: hexc("#6e5c78"), 1: hexc("#8a74a0"), 2: hexc("#4a3b54")}[state]
    rounded(im, 0, 0, w, h, INK[0])
    if state != 2:
        rect(im, 1, h - 3, w - 2, 2, hexc("#1a1320"))
    rect(im, 1, 1 + off, w - 2, h - 4, face)
    hline(im, 2, 1 + off, w - 4, hi)
    if state == 1:
        frame_rect(im, 1, 1, w - 2, h - 3, ACC[3])
        hline(im, 2, 2, w - 4, hi)
    return im


def button_accent(state):
    w, h = 16, 14
    im = new(w, h)
    off = 1 if state == 2 else 0
    face = {0: ACC[2], 1: ACC[3], 2: ACC[1]}[state]
    rounded(im, 0, 0, w, h, INK[0])
    if state != 2:
        rect(im, 1, h - 3, w - 2, 2, ACC[0])
    rect(im, 1, 1 + off, w - 2, h - 4, face)
    hline(im, 2, 1 + off, w - 4, ACC[4])
    return im


def tab(active):
    w, h = 16, 12
    im = new(w, h)
    if active:
        rect(im, 1, 0, w - 2, h, INK[0])
        rect(im, 0, 1, w, h - 1, INK[0])
        rect(im, 1, 1, w - 2, h - 1, UI_BG)
        hline(im, 2, 1, w - 4, ACC[3])
        hline(im, 2, 2, w - 4, alpha(ACC[3], 90))
    else:
        rect(im, 1, 2, w - 2, h - 2, INK[0])
        rect(im, 0, 3, w, h - 3, INK[0])
        rect(im, 1, 3, w - 2, h - 4, UI_BG2)
        hline(im, 2, 3, w - 4, UI_LINE)
        hline(im, 0, h - 1, w, UI_LINE)
    return im


def textfield(focus):
    w, h = 16, 14
    im = new(w, h)
    rounded(im, 0, 0, w, h, ACC[3] if focus else UI_LINE)
    rect(im, 1, 1, w - 2, h - 2, hexc("#140f18"))
    hline(im, 1, 1, w - 2, INK[0])
    return im


def caret(on):
    im = new(2, 9)
    if on:
        rect(im, 0, 0, 1, 9, ACC[4])
        rect(im, 1, 0, 1, 9, alpha(ACC[4], 70))
    return im


def checkbox(on, radio=False):
    im = new(9, 9)
    if radio:
        ellipse(im, 4.5, 4.5, 4.5, 4.5, INK[0])
        ellipse(im, 4.5, 4.5, 3.5, 3.5, UI_LINE)
        ellipse(im, 4.5, 4.5, 2.6, 2.6, hexc("#140f18"))
        if on:
            ellipse(im, 4.5, 4.5, 1.8, 1.8, ACC[3])
            px(im, 4, 3, ACC[4])
    else:
        rounded(im, 0, 0, 9, 9, INK[0])
        rect(im, 1, 1, 7, 7, UI_LINE)
        rect(im, 2, 2, 5, 5, hexc("#140f18"))
        if on:
            rect(im, 1, 1, 7, 7, ACC[3])
            hline(im, 1, 1, 7, ACC[4])
            for (x, y) in ((2, 4), (3, 5), (4, 6), (5, 5), (6, 4), (7, 3), (6, 3)):
                px(im, x, y, WHITE)
            px(im, 3, 6, INK[1])
    return im


def scroll_track():
    im = new(6, 12)
    rect(im, 0, 0, 6, 12, hexc("#140f18"))
    vline(im, 0, 0, 12, UI_LINE)
    vline(im, 5, 0, 12, UI_LINE)
    return im


def scroll_thumb(hover=False):
    im = new(6, 12)
    rounded(im, 0, 0, 6, 12, INK[0])
    rect(im, 1, 1, 4, 10, ACC[3] if hover else hexc("#6e5c78"))
    vline(im, 1, 1, 10, ACC[4] if hover else hexc("#8a74a0"))
    for y in (4, 6, 8):
        hline(im, 2, y, 2, INK[1])
    return im


def row_highlight():
    im = new(12, 10)
    rect(im, 0, 0, 12, 10, alpha(ACC[3], 60))
    vline(im, 0, 0, 10, ACC[3])
    hline(im, 1, 0, 11, alpha(ACC[4], 50))
    return im


def tag():
    w, h = 9, 11
    im = new(w, h)
    rounded(im, 0, 0, w, h, INK[0])
    rect(im, 1, 1, w - 2, h - 2, alpha(UI_BG, 235))
    hline(im, 1, 1, w - 2, alpha(UI_HI, 200))
    return im


def bubble_body():
    s = 15
    im = new(s, s)
    rounded(im, 0, 0, s, s, INK[1])
    rounded(im, 1, 1, s - 2, s - 2, WHITE)
    hline(im, 2, s - 2, s - 4, C["cream"][3])
    return im


def bubble_tail():
    im = new(9, 6)
    stamp(im, 0, 0, [
        "#WWWWWWW#",
        ".#WWWWW#.",
        "..#WWW#..",
        "...#W#...",
        "....#....",
    ], {"#": INK[1], "W": WHITE})
    return im


EMOTES = ["question", "exclaim", "dots", "z", "heart", "sweat", "sparkle", "music", "check", "cross", "coffee", "bulb"]


def emote(kind, frame=0):
    w, h = 13, 15
    im = new(w, h)
    rounded(im, 0, 0, w, 11, INK[1])
    rounded(im, 1, 1, w - 2, 9, WHITE)
    hline(im, 2, 9, w - 4, C["cream"][3])
    stamp(im, 4, 10, ["#W#.", ".##.", ".#.."], {"#": INK[1], "W": WHITE})
    ox, oy = 2, 2
    if kind == "question":
        stamp(im, 4, 2, [".##.", "#..#", "..#.", ".#..", "....", ".#.."], {"#": ACC[2]})
    elif kind == "exclaim":
        stamp(im, 5, 2, ["##", "##", "##", "#.", "..", "##"], {"#": C["red"][3]})
    elif kind == "dots":
        for i in range(3):
            c = INK[2] if i <= frame else C["cream"][2]
            rect(im, 3 + i * 3, 5, 2, 2, c)
    elif kind == "z":
        zs = [["###", "..#", ".#.", "#..", "###"]]
        stamp(im, 3 + frame, 2 + (2 - frame), zs[0], {"#": C["blue"][3]})
        if frame >= 1:
            stamp(im, 8, 2, ["##", ".#", "##"], {"#": C["blue"][4]})
    elif kind == "heart":
        stamp(im, 3, 2, [".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."],
              {"#": C["red"][3]})
        px(im, 4, 3, C["pink"][4])
    elif kind == "sweat":
        stamp(im, 5, 2, ["..#", ".##", "###", "###", ".#."], {"#": C["blue"][4]})
        px(im, 6, 5, WHITE)
    elif kind == "sparkle":
        big = frame % 2 == 0
        cx, cy = 6, 5
        col = C["yellow"][3]
        for d in range(-3 if big else -2, 4 if big else 3):
            px(im, cx + d, cy, col)
            px(im, cx, cy + d, col)
        px(im, cx, cy, WHITE)
        px(im, 3 if big else 9, 3 if big else 7, C["yellow"][4])
    elif kind == "music":
        stamp(im, 3 + frame, 2, ["..####", "..#..#", "..#..#", ".##.##", "###.##", ".#...."][:6],
              {"#": C["purple"][3]})
    elif kind == "check":
        for (x, y) in ((3, 5), (4, 6), (5, 7), (6, 6), (7, 5), (8, 4), (9, 3)):
            px(im, x, y, C["green"][3])
            px(im, x, y - 1, C["green"][4])
    elif kind == "cross":
        for d in range(5):
            px(im, 4 + d, 3 + d, C["red"][3])
            px(im, 8 - d, 3 + d, C["red"][3])
    elif kind == "coffee":
        rect(im, 4, 4, 4, 4, C["walnut"][2])
        px(im, 8, 5, C["walnut"][2])
        px(im, 5, 2 - 0, alpha(ST[4], 200))
        px(im, 6, 3, alpha(ST[4], 200))
    elif kind == "bulb":
        ellipse(im, 6.5, 4.5, 2.6, 2.6, C["yellow"][3])
        rect(im, 5, 7, 3, 2, ST[4])
        px(im, 5, 3, WHITE)
    return im


ICONS = ["cpu", "ram", "gpu", "disk", "net", "temp", "fan", "question", "check", "cross",
         "close", "terminal", "file", "globe", "search", "plan", "chat", "clock", "cat",
         "code", "power", "user", "door", "bolt"]


def icon(name, size=9):
    im = new(size, size)
    c = TEXT
    a = ACC[3]
    G = {
        "cpu": [".#.#.#.", "#######", ".#AAA#.", "##AAA##", ".#AAA#.", "#######", ".#.#.#."],
        "ram": ["#######", "#A#A#A#", "#A#A#A#", "#######", "#.#.#.#"],
        "gpu": ["########", "#.AAA..#", "#AA.AA.#", "#.AAA..#", "########", "..#..#.."],
        "disk": [".#####.", "#.....#", "#..A..#", "#.....#", "#....##", ".#####."],
        "net": ["...#...", "..###..", "...#...", "#######", "#..#..#", "#..#..#", "A..A..A"],
        "temp": ["..#..", ".#.#.", ".#A#.", ".#A#.", ".#A#.", "#AAA#", "#AAA#", ".###."],
        "fan": [".##.#..", "..##...", "#.#A.##", "##.A.#.", "..#.##.", "..##...", "....##."],
        "question": [".###.", "#...#", "...#.", "..#..", ".....", "..#.."],
        "check": [".......#", "......##", "#....##.", "##..##..", ".####...", "..##...."],
        "cross": ["#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".#...#.", "#.....#"],
        "close": ["##...##", ".##.##.", "..###..", ".##.##.", "##...##"],
        "terminal": ["#######", "#.....#", "#A....#", "#.A...#", "#A.##.#", "#######"],
        "file": ["####..", "#..##.", "#...##", "#.AA.#", "#....#", "#.AA.#", "######"],
        "globe": [".###.", "#.#.#", "#####", "#.#.#", ".###."],
        "search": [".###..", "#...#.", "#...#.", "#...#.", ".####.", "....##"],
        "plan": ["#.###", ".....", "#.###", ".....", "A.###"],
        "chat": ["######", "#....#", "#.AA.#", "######", ".#....", "#....."],
        "clock": [".###.", "#.#.#", "#.##.", "#...#", ".###."],
        "cat": ["#...#", "##.##", "#####", "#A#A#", "#####", ".###."],
        "code": ["..#...#..", ".#.....#.", "#...#...#", ".#..#..#.", "..#.#.#.."],
        "power": ["..#..", "#.#.#", "#.#.#", "#...#", ".###."],
        "user": [".###.", ".###.", "..#..", "#####", "#####"],
        "door": ["#####", "#...#", "#...#", "#..A#", "#...#", "#####"],
        "bolt": ["..##", ".##.", "####", ".##.", "##..", "#..."],
    }
    g = G[name]
    gh = len(g)
    gw = max(len(r) for r in g)
    ox = (size - gw) // 2
    oy = (size - gh) // 2
    stamp(im, ox, oy, g, {"#": c, "A": a})
    return im


def bar_bg():
    im = new(12, 7)
    rounded(im, 0, 0, 12, 7, INK[0])
    rect(im, 1, 1, 10, 5, hexc("#140f18"))
    hline(im, 1, 1, 10, hexc("#0c0910"))
    return im


def bar_fill(level):
    col = {0: C["code"][0], 1: C["yellow"][3], 2: C["red"][3], 3: C["sky"][1], 4: ACC[3]}[level]
    dark = {0: C["teal"][3], 1: C["yellow"][1], 2: C["red"][1], 3: C["blue"][3], 4: ACC[1]}[level]
    im = new(12, 5)
    rect(im, 0, 0, 12, 5, col)
    hline(im, 0, 0, 12, alpha(WHITE, 120))
    hline(im, 0, 4, 12, dark)
    return im


def led(state, w=5, h=2):
    im = new(w, h)
    col = {0: hexc("#1e1824"), 1: C["code"][0], 2: C["yellow"][3], 3: C["red"][3], 4: C["sky"][2]}[state]
    rect(im, 0, 0, w, h, col)
    if state:
        px(im, 0, 0, alpha(WHITE, 120))
    return im


def fan(frame, r=9):
    s = r * 2 + 2
    im = new(s, s)
    c = r + 1
    ellipse(im, c, c, r + 0.5, r + 0.5, INK[0])
    ellipse(im, c, c, r - 0.5, r - 0.5, hexc("#1a1420"))
    for k in range(5):
        a = k * 2 * math.pi / 5 + frame * (2 * math.pi / 5) / 4
        for t in range(2, r - 1):
            aa = a + t * 0.12
            x = c + math.cos(aa) * t
            y = c + math.sin(aa) * t
            px(im, int(x), int(y), ST[4])
            px(im, int(x + math.cos(aa + 1.4)), int(y + math.sin(aa + 1.4)), ST[3])
    ellipse(im, c, c, 2.2, 2.2, ST[5])
    px(im, c - 1, c - 1, ST[7])
    return im


def sysroom():
    """PC case cut-away, 232x150. Returns image and named regions."""
    w, h = 232, 150
    im = new(w, h)
    # case shell
    rounded(im, 0, 0, w, h, INK[0])
    rect(im, 1, 1, w - 2, h - 2, ST[2])
    hline(im, 1, 1, w - 2, ST[4])
    vline(im, 1, 1, h - 2, ST[3])
    # interior + motherboard
    rect(im, 5, 5, w - 10, h - 10, hexc("#16121c"))
    mb = hexc("#1f3a34")
    rect(im, 9, 9, 150, 98, mb)
    for x in range(12, 156, 6):
        for y in range(12, 104, 6):
            px(im, x, y, hexc("#2a4a42"))
    # traces
    for i in range(6):
        line(im, 60, 30 + i * 3, 100, 30 + i * 3, hexc("#2e5a4c"))
    for i in range(5):
        line(im, 30 + i * 3, 62, 30 + i * 3, 100, hexc("#2e5a4c"))
    R = {}
    # CPU socket: an LED column grid lives inside
    rect(im, 18, 14, 52, 44, ST[3])
    frame_rect(im, 18, 14, 52, 44, ST[5])
    rect(im, 21, 17, 46, 38, hexc("#0e0b12"))
    R["cpuCores"] = [22, 18, 44, 36]
    R["cpuLabel"] = [18, 59]
    # RAM slots right of the socket
    for i in range(4):
        x = 80 + i * 9
        rect(im, x, 14, 6, 60, hexc("#0e0b12"))
        frame_rect(im, x, 14, 6, 60, ST[3])
        px(im, x + 2, 13, C["yellow"][3])
        px(im, x + 3, 13, C["yellow"][3])
    R["ramSticks"] = [[81 + i * 9, 15, 4, 58] for i in range(4)]
    R["ramLabel"] = [80, 76]
    # NVMe slot
    rect(im, 18, 66, 52, 12, hexc("#0e0b12"))
    frame_rect(im, 18, 66, 52, 12, ST[3])
    for x in range(22, 64, 8):
        rect(im, x, 69, 5, 6, ST[2])
    R["disk"] = [20, 68, 48, 8]
    R["diskLed"] = [66, 71]
    R["diskLabel"] = [18, 80]
    # GPU card
    rect(im, 9, 108, 150, 32, ST[1])
    frame_rect(im, 9, 108, 150, 32, ST[3])
    hline(im, 10, 109, 148, ST[4])
    rect(im, 9, 106, 34, 2, C["yellow"][2])
    R["gpuFans"] = [[30, 112], [70, 112]]  # top-left of a 20x20 fan frame
    R["gpuBar"] = [100, 116, 52, 6]
    R["gpuMemBar"] = [100, 126, 52, 6]
    R["gpuLabel"] = [100, 109]
    # PSU at the bottom right + its fan
    rect(im, 166, 98, 58, 44, ST[1])
    frame_rect(im, 166, 98, 58, 44, ST[3])
    R["psuFan"] = [185, 110]
    # IO / network ports on the right edge
    rect(im, 166, 9, 58, 40, ST[1])
    frame_rect(im, 166, 9, 58, 40, ST[3])
    for i in range(2):
        rect(im, 172 + i * 24, 16, 16, 12, INK[0])
        rect(im, 174 + i * 24, 18, 12, 7, hexc("#2a2430"))
        for k in range(4):
            px(im, 176 + i * 24 + k * 2, 18, C["yellow"][2])
    R["netRxLed"] = [173, 30]
    R["netTxLed"] = [182, 30]
    R["net2RxLed"] = [197, 30]
    R["net2TxLed"] = [206, 30]
    R["netLabel"] = [170, 36]
    # thermometer
    rect(im, 172, 54, 46, 40, hexc("#1a1420"))
    frame_rect(im, 172, 54, 46, 40, ST[3])
    rect(im, 192, 58, 6, 26, INK[0])
    rect(im, 193, 59, 4, 24, hexc("#0e0b12"))
    ellipse(im, 195, 87, 4.5, 4.5, INK[0])
    ellipse(im, 195, 87, 3.5, 3.5, C["red"][3])
    for y in range(60, 84, 4):
        hline(im, 199, y, 3, ST[5])
    R["thermoTube"] = [193, 59, 4, 24]
    R["thermoBulb"] = [195, 87]
    R["tempLabel"] = [174, 56]
    # cables
    for i, c in enumerate((C["red"][2], C["yellow"][2], INK[2])):
        line(im, 160, 120 + i * 2, 166, 120 + i * 2, c)
    # screws
    for (x, y) in ((4, 4), (w - 5, 4), (4, h - 5), (w - 5, h - 5)):
        px(im, x, y, ST[5])
    # glass reflection
    for k in range(0, 120):
        x = 130 + k // 3
        y = k
        if y < h - 6 and x < w - 6:
            px(im, x, y + 5, alpha(WHITE, 14))
    return im, R


def build():
    single("ui_panel_dark", panel_dark(), slice=[8, 8, 8, 8])
    single("ui_panel_light", panel_light(), slice=[8, 8, 8, 8])
    s = Sheet("ui_button", 16, 14)
    for i, n in enumerate(("normal", "hover", "pressed")):
        s.add(button(i), n)
    s.extra["slice"] = [5, 4, 5, 5]
    s.save()
    s = Sheet("ui_button_accent", 16, 14)
    for i, n in enumerate(("normal", "hover", "pressed")):
        s.add(button_accent(i), n)
    s.extra["slice"] = [5, 4, 5, 5]
    s.save()
    s = Sheet("ui_tab", 16, 12)
    s.add(tab(True), "active")
    s.add(tab(False), "inactive")
    s.extra["slice"] = [4, 4, 4, 2]
    s.save()
    s = Sheet("ui_textfield", 16, 14)
    s.add(textfield(False), "normal")
    s.add(textfield(True), "focus")
    s.extra["slice"] = [4, 4, 4, 4]
    s.save()
    s = Sheet("ui_caret", 2, 9)
    s.add(caret(True), "on")
    s.add(caret(False), "off")
    s.anim("blink", [0, 1], 2)
    s.save()
    s = Sheet("ui_check", 9, 9)
    s.add(checkbox(False), "off")
    s.add(checkbox(True), "on")
    s.add(checkbox(False, True), "radio_off")
    s.add(checkbox(True, True), "radio_on")
    s.save()
    single("ui_scroll_track", scroll_track(), slice=[0, 3, 0, 3])
    s = Sheet("ui_scroll_thumb", 6, 12)
    s.add(scroll_thumb(False), "normal")
    s.add(scroll_thumb(True), "hover")
    s.extra["slice"] = [0, 3, 0, 3]
    s.save()
    single("ui_row_highlight", row_highlight(), slice=[2, 2, 2, 2])
    single("ui_tag", tag(), slice=[3, 3, 3, 3])
    single("bubble", bubble_body(), slice=[5, 5, 5, 5])
    single("bubble_tail", bubble_tail(), anchor=(4, 0))
    s = Sheet("emote", 13, 15, cols=8, anchor=(6, 14))
    idx = {}
    for k in EMOTES:
        frames = 3 if k in ("dots", "z") else (2 if k in ("sparkle", "music") else 1)
        idx[k] = [s.add(emote(k, f), k if f == 0 else None) for f in range(frames)]
    for k, fr in idx.items():
        s.anim(k, fr, 3 if k in ("dots", "z") else 4, True)
    s.save()
    s = Sheet("icons", 9, 9, cols=12)
    for n in ICONS:
        s.add(icon(n), n)
    s.save()
    single("ui_bar_bg", bar_bg(), slice=[3, 3, 3, 3])
    s = Sheet("ui_bar_fill", 12, 5)
    for i, n in enumerate(("ok", "warn", "hot", "info", "accent")):
        s.add(bar_fill(i), n)
    s.extra["slice"] = [1, 1, 1, 1]
    s.save()
    s = Sheet("ui_led", 5, 2)
    for i, n in enumerate(("off", "ok", "warn", "hot", "info")):
        s.add(led(i), n)
    s.save()
    s = Sheet("ui_led_dot", 2, 2)
    for i, n in enumerate(("off", "ok", "warn", "hot", "info")):
        s.add(led(i, 2, 2), n)
    s.save()
    s = Sheet("fan", 20, 20, anchor=(0, 0))
    for f in range(4):
        s.add(fan(f))
    s.anim("spin", [0, 1, 2, 3], 12)
    s.save()
    img, regions = sysroom()
    single("sysroom", img, regions=regions)
