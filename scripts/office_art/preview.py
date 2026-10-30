# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reference renderer: composes the office purely from layout.json +
manifest.json the way QML is expected to (docs/office-art.md)."""

import json

from PIL import Image, ImageDraw, ImageFont

from .core import OUT, FONT

TEXT = (242, 232, 220, 255)
DIM = (156, 142, 166, 255)
ACC = (240, 162, 74, 255)


class Art:
    def __init__(self):
        self.man = json.loads((OUT / "manifest.json").read_text())["sheets"]
        self.lay = json.loads((OUT / "layout.json").read_text())
        self.img = {}
        self.font = ImageFont.truetype(str(FONT), 16)

    def sheet(self, name):
        s = self.man[name]
        if s["file"] not in self.img:
            self.img[s["file"]] = Image.open(OUT / s["file"]).convert("RGBA")
        return s, self.img[s["file"]]

    def index(self, s, frame):
        if frame is None:
            return 0
        if isinstance(frame, str):
            return s["frameNames"][frame]
        return frame

    def frame(self, name, frame=None, variant=0, t=0.0, anim=None):
        s, im = self.sheet(name)
        if anim:
            a = s["animations"][anim]
            n = len(a["frames"])
            k = int(t * a["fps"])
            k = k % n if a["loop"] else min(k, n - 1)
            idx = a["frames"][k]
        else:
            idx = self.index(s, frame)
        fw, fh = s["frameWidth"], s["frameHeight"]
        if "variants" in s:
            x, y = idx * fw, variant * fh
        else:
            c = s["columns"]
            x, y = (idx % c) * fw, (idx // c) * fh
        return im.crop((x, y, x + fw, y + fh)), s["anchor"]

    def draw(self, dst, name, x, y, frame=None, variant=0, t=0.0, anim=None, opacity=1.0):
        f, (ax, ay) = self.frame(name, frame, variant, t, anim)
        if opacity < 1.0:
            a = f.getchannel("A").point(lambda v: int(v * opacity))
            f.putalpha(a)
        paste(dst, f, int(round(x - ax)), int(round(y - ay)))

    def nine(self, dst, name, x, y, w, h, frame=None):
        s, _ = self.sheet(name)
        f, _ = self.frame(name, frame)
        l, t, r, b = s["slice"]
        fw, fh = f.size
        xs = [(0, l, 0, l), (l, fw - r, l, w - r), (fw - r, fw, w - r, w)]
        ys = [(0, t, 0, t), (t, fh - b, t, h - b), (fh - b, fh, h - b, h)]
        for (sx0, sx1, dx0, dx1) in xs:
            for (sy0, sy1, dy0, dy1) in ys:
                if sx1 <= sx0 or sy1 <= sy0 or dx1 <= dx0 or dy1 <= dy0:
                    continue
                part = f.crop((sx0, sy0, sx1, sy1)).resize((dx1 - dx0, dy1 - dy0), Image.NEAREST)
                paste(dst, part, x + dx0, y + dy0)

    def text(self, dst, x, y, s, col=TEXT):
        d = ImageDraw.Draw(dst)
        d.fontmode = "1"
        d.text((x, y - 4), s, font=self.font, fill=col)

    def text_w(self, s):
        return int(self.font.getlength(s))


def paste(dst, src, x, y):
    if x >= dst.width or y >= dst.height or x + src.width <= 0 or y + src.height <= 0:
        return
    cx0, cy0 = max(0, -x), max(0, -y)
    src = src.crop((cx0, cy0, min(src.width, dst.width - x), min(src.height, dst.height - y)))
    dst.alpha_composite(src, (x + cx0, y + cy0))


# preview cast: who is where doing what
DESK_POSES = {
    0: ("sit_type_down", 1), 1: ("sit_think_down", 2), 2: ("sit_type_up", 3), 3: ("sit_read_up", 4),
    4: ("sit_cheer_down", 5), 5: ("sit_read_down", 6), 6: ("sit_facepalm_up", 7), 7: ("sit_type_up", 2),
}
SCREENS = {2: "code_a", 3: "doc", 6: "error", 7: "terminal_a", 0: None, 1: None, 4: None, 5: None}
GLOWS = {2: "cool", 3: "warm", 6: "red", 7: "green", 0: "cool", 1: "cool", 4: "green", 5: "warm"}
NAMES = {0: "Explore 1", 1: "Plan 1", 2: "General 1", 3: "Explore 2", 4: "General 2", 5: "Explore 3",
         6: "General 3", 7: "Plan 2"}


def scene(out, t=0.35, panels=True, scale=3, tod="day"):
    A = Art()
    TOD[0] = tod
    L = A.lay
    W, H = L["size"]
    im = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    nodes = {n["id"]: (n["x"], n["y"]) for n in L["graph"]["nodes"]}
    props = {p["id"]: p for p in L["props"]}

    # per-desk monitor frames
    for d in L["desks"]:
        if d["facing"] == "up":
            props[d["monitor"]]["frame"] = SCREENS[d["index"]]

    actors = []  # (z, draw fn)

    def actor(z, name, x, y, anim, variant, shadow=True, mirror=False):
        actors.append((z, ("char", name, x, y, anim, variant, shadow)))

    for d in L["desks"]:
        pose, look = DESK_POSES[d["index"]]
        s = d["seat"]
        actor(s["z"], "characters", s["x"], s["y"], pose, look, shadow=False)
    ls = L["lead"]["seat"]
    actor(ls["z"], "characters", ls["x"], ls["y"], "sit_talk_down", 0, shadow=False)
    # walkers, queue, lounge
    walkers = [("walk_right", 3, nodes["c_156"][0] - 8, 120), ("walk_up", 6, 228, 168),
               ("question_up", 1, *nodes["queue0"]), ("idle_up", 7, *nodes["queue1"]),
               ("drink", 2, nodes["coffee"][0], nodes["coffee"][1] + 2), ("wave", 4, nodes["entry_in"][0], 240),
               ("talk_up", 5, *nodes["lead_visit0"])]
    for (anim, look, x, y) in walkers:
        actor(y, "characters", x, y, anim, look)
    lounge = {l["id"]: l for l in L["lounge"]}
    for sid, anim, look in (("sofa0", "lounge_sip_down", 5), ("armchair_l", "lounge_sleep_right", 2)):
        l = lounge[sid]
        actor(l["z"] + 0.1, "characters", l["x"], l["y"], anim, look, shadow=False)
    cx, cy = nodes["cat_rug"]
    actors.append((cy, ("cat", cx, cy)))

    for layer in L["layers"]:
        if layer == "objects":
            items = [(p["z"], ("prop", p)) for p in L["props"] if p["layer"] == "objects"] + actors
            items.sort(key=lambda it: (it[0], 0 if it[1][0] == "prop" else 1))
            for z, it in items:
                if it[0] == "prop":
                    draw_prop(A, im, it[1], t)
                elif it[0] == "char":
                    _, name, x, y, anim, variant, sh = it
                    A.draw(im, name, x, y, variant=variant, anim=anim, t=t)
                else:
                    A.draw(im, "cat", it[1], it[2], anim="sit", t=t)
            continue
        if layer == "shadow":
            for p in L["props"]:
                if p["layer"] == "shadow":
                    draw_prop(A, im, p, t)
            for z, it in actors:
                if it[0] == "char" and it[6]:
                    A.draw(im, "shadow_person", it[2], it[3])
                elif it[0] == "cat":
                    A.draw(im, "shadow_cat", it[1], it[2])
            continue
        for p in L["props"]:
            if p["layer"] == layer:
                draw_prop(A, im, p, t)
        for li in L["lights"]:
            if li["layer"] != layer:
                continue
            if li.get("when") and "closed" in li["when"]:
                continue
            fr = tod if li.get("timeOfDay") else li.get("frame")
            op = li.get("night" if tod == "night" else "day", 1.0)
            A.draw(im, li["sprite"], li["x"], li["y"], frame=fr, anim=li.get("animation"), t=t, opacity=op)
        if layer == "light" and tod == "night":
            tint = Image.new("RGBA", im.size, NIGHT_TINT)
            im.alpha_composite(tint)
            for li in L["lights"]:
                if li["layer"] == "light":
                    fr = li.get("frame")
                    A.draw(im, li["sprite"], li["x"], li["y"], frame=fr, anim=li.get("animation"), t=t, opacity=li.get("night", 1.0))
        if layer == "light":
            for d in L["desks"]:
                g = d["glow"]
                A.draw(im, g["sprite"], g["x"], g["y"], frame=GLOWS[d["index"]], opacity=0.9)
            g = L["lead"]["glow"]
            A.draw(im, g["sprite"], g["x"], g["y"], frame="cool", opacity=0.8)
        if layer == "ambient":
            for (x, y) in ((214, 92), (232, 120), (250, 104), (300, 112), (318, 90)):
                A.draw(im, "dust", x, y, anim="twinkle", t=t + x * 0.01)
        if layer == "ui":
            for d in L["desks"]:
                if d["index"] in (2,):
                    lab = d["label"]
                    tag(A, im, lab["x"], lab["y"], NAMES[d["index"]], d["index"])
            tag(A, im, L["lead"]["label"]["x"], L["lead"]["label"]["y"], "Lead", -1)
            bx, by = nodes["queue0"]
            A.draw(im, "emote", bx + 9, by - 30, anim="question", t=t)
            l = lounge["armchair_l"]
            A.draw(im, "emote", l["x"] + 8, l["y"] - 28, anim="z", t=t + 0.4)
            d6 = L["desks"][6]["bubble"]
            A.draw(im, "emote", d6["x"], d6["y"], anim="sweat", t=t)
            d4 = L["desks"][4]["bubble"]
            A.draw(im, "emote", d4["x"], d4["y"], anim="sparkle", t=t)
            A.draw(im, "emote", nodes["entry_in"][0] + 9, 210, anim="heart", t=t)
    clean = im.copy()
    if panels:
        overlay_panels(A, im)
    big = im.resize((W * scale, H * scale), Image.NEAREST)
    big.save(out)
    clean.resize((W * scale, H * scale), Image.NEAREST).save(str(out).replace(".png", "-clean.png"))
    return im


NIGHT_TINT = (16, 18, 48, 150)
TOD = ["day"]


def draw_prop(A, im, p, t):
    fr = p.get("frame")
    if p.get("timeOfDay"):
        fr = TOD[0]
    A.draw(im, p["sprite"], p["x"], p["y"], frame=fr, anim=p.get("animation"), t=t)
    if p.get("clock"):
        A.draw(im, p["clock"]["hour"], p["x"], p["y"], frame=10)
        A.draw(im, p["clock"]["minute"], p["x"], p["y"], frame=2)


def tag(A, im, cx, y, text, idx):
    w = A.text_w(text) + 10
    x = int(cx - w / 2)
    A.nine(im, "ui_tag", x, y, w, 11)
    col = {-1: (240, 162, 74, 255), 6: (236, 127, 94, 255), 4: (127, 216, 174, 255)}.get(idx, (127, 216, 174, 255))
    A.draw(im, "ui_led_dot", x + 3, y + 4, frame="ok" if idx not in (6,) else "hot")
    A.text(im, x + 7, y + 2, text, TEXT)


CODE = [
    ("def ", "render(self, pose):"),
    ("    f = ", "pose.get('facing')"),
    ("    if ", "f == 'left':"),
    ("        return ", "mirror(...)"),
    ("    body = ", "new(FW, FH)"),
    ("    draw_legs(", "body, lk, f)"),
    ("    draw_torso(", "body, lk, f)"),
    ("    outline(", "body, OUTLINE)"),
]


def overlay_panels(A, im):
    # zoomed monitor, top right
    bx, by = 6, 6
    A.draw(im, "monitor_big", bx, by)
    sx, sy, sw, sh = A.man["monitor_big"]["screen"]
    A.text(im, bx + sx + 4, by + sy + 3, "props.py", (127, 184, 240, 255))
    A.text(im, bx + sx + sw - 60, by + sy + 3, "General 1", DIM)
    for i, (a, b) in enumerate(CODE):
        y = by + sy + 16 + i * 10
        A.text(im, bx + sx + 4, y, "%2d" % (40 + i), (90, 80, 104, 255))
        A.text(im, bx + sx + 20, y, a, (240, 138, 138, 255) if "def" in a or "if" in a or "return" in a else TEXT)
        A.text(im, bx + sx + 20 + A.text_w(a), y, b, (127, 216, 174, 255))
    A.draw(im, "ui_caret", bx + sx + 20 + A.text_w(CODE[-1][0] + CODE[-1][1]) + 1, by + sy + 16 + 7 * 10 + 1, frame="on")
    A.draw(im, "monitor_big_glass", bx + sx, by + sy)
    # system panel, bottom right
    px0, py0 = 236, 94
    pw, ph = 240, 172
    A.nine(im, "ui_panel_dark", px0, py0, pw, ph)
    A.draw(im, "icons", px0 + 6, py0 + 6, frame="cpu")
    A.text(im, px0 + 18, py0 + 6, "System  load 3.2  cpu 61%", TEXT)
    A.draw(im, "icons", px0 + pw - 14, py0 + 6, frame="close")
    sys_img, _ = A.frame("sysroom")
    small = sys_img.crop((0, 0, 232, 150))
    reg = A.man["sysroom"]["regions"]
    room = small.copy()
    # CPU core LED columns
    x0, y0, w0, h0 = reg["cpuCores"]
    loads = [80, 45, 95, 30, 60, 20, 70, 55]
    for i, ld in enumerate(loads):
        segs = h0 // 3
        on = int(segs * ld / 100)
        for k in range(segs):
            st = "off"
            if k < on:
                st = "hot" if k > segs * 0.8 else ("warn" if k > segs * 0.55 else "ok")
            f, _ = A.frame("ui_led", frame=st)
            paste(room, f, x0 + i * 5 + 2, y0 + h0 - 3 - k * 3)
    for i, (x, y, w, h) in enumerate(reg["ramSticks"]):
        fill = [0.9, 0.8, 0.4, 0.0][i]
        for k in range(h // 3):
            st = "info" if k < int(h / 3 * fill) else "off"
            f, _ = A.frame("ui_led", frame=st)
            paste(room, f.crop((0, 0, 4, 2)), x, y + h - 3 - k * 3)
    for (x, y) in reg["gpuFans"]:
        f, _ = A.frame("fan", anim="spin", t=0.1)
        paste(room, f, x, y)
    f, _ = A.frame("fan", anim="spin", t=0.3)
    paste(room, f, *reg["psuFan"])
    gx, gy, gw, gh = reg["gpuBar"]
    nine_into(A, room, "ui_bar_bg", gx, gy, gw, gh + 1)
    nine_into(A, room, "ui_bar_fill", gx + 1, gy + 1, int((gw - 2) * 0.72), gh - 1, "warn")
    gx, gy, gw, gh = reg["gpuMemBar"]
    nine_into(A, room, "ui_bar_bg", gx, gy, gw, gh + 1)
    nine_into(A, room, "ui_bar_fill", gx + 1, gy + 1, int((gw - 2) * 0.4), gh - 1, "info")
    tx, ty, tw, th = reg["thermoTube"]
    fill = int(th * 0.62)
    for yy in range(ty + th - fill, ty + th):
        for xx in range(tx, tx + tw):
            room.putpixel((xx, yy), (208, 80, 63, 255))
    for key in ("netRxLed", "netTxLed", "net2RxLed"):
        f, _ = A.frame("ui_led_dot", frame="ok")
        paste(room, f, *reg[key])
    f, _ = A.frame("ui_led_dot", frame="warn")
    paste(room, f, *reg["diskLed"])
    room = room.resize((174, 113), Image.NEAREST) if False else room
    # the panel shows the case cut-away at 1x, cropped to fit
    paste(im, room, px0 + 4, py0 + 18)
    # question panel, bottom left
    qx, qy, qw, qh = 6, 152, 226, 112
    A.nine(im, "ui_panel_dark", qx, qy, qw, qh)
    tx = qx + 6
    for i, (lab, act) in enumerate((("Layout", True), ("Palette", False), ("Submit", False))):
        w = A.text_w(lab) + 12
        A.nine(im, "ui_tab", tx, qy + 5, w, 12, "active" if act else "inactive")
        A.text(im, tx + 6, qy + 8, lab, TEXT if act else DIM)
        tx += w + 2
    A.draw(im, "icons", qx + qw - 14, qy + 6, frame="close")
    A.draw(im, "icons", qx + 6, qy + 22, frame="question")
    A.text(im, qx + 18, qy + 22, "Where should the lounge go?", TEXT)
    opts = [("Right side", "next to the bookshelf"), ("Bottom left", "by the entrance"),
            ("Center", "between the pods")]
    for i, (a, b) in enumerate(opts):
        y = qy + 36 + i * 13
        if i == 0:
            A.nine(im, "ui_row_highlight", qx + 4, y - 2, qw - 16, 12)
        A.text(im, qx + 8, y, "%d." % (i + 1), ACC if i == 0 else DIM)
        A.text(im, qx + 22, y, a, TEXT)
        A.text(im, qx + 26 + A.text_w(a), y, b, DIM)
    y = qy + 36 + 3 * 13
    A.text(im, qx + 8, y, "4.", DIM)
    A.nine(im, "ui_textfield", qx + 20, y - 3, qw - 34, 14, "focus")
    A.text(im, qx + 24, y, "Type something.", DIM)
    A.draw(im, "ui_caret", qx + 24 + A.text_w("Type something.") + 1, y - 1, frame="on")
    A.nine(im, "ui_scroll_track", qx + qw - 10, qy + 34, 6, 50)
    A.nine(im, "ui_scroll_thumb", qx + qw - 10, qy + 34, 6, 20, "normal")
    by = qy + qh - 16
    A.nine(im, "ui_button_accent", qx + qw - 62, by, 56, 14, "normal")
    A.text(im, qx + qw - 54, by + 3, "Answer", (40, 24, 20, 255))
    tw = A.text_w("In terminal") + 12
    A.nine(im, "ui_button", qx + qw - 66 - tw, by, tw, 14, "hover")
    A.text(im, qx + qw - 60 - tw, by + 3, "In terminal", TEXT)
    A.draw(im, "ui_check", qx + 8, by + 3, frame="on")
    A.text(im, qx + 20, by + 3, "multi", DIM)


def nine_into(A, dst, name, x, y, w, h, frame=None):
    A.nine(dst, name, x, y, w, h, frame)


def character_sheet(out, scale=4):
    """One row per animation; per look, its distinct frames in play order."""
    A = Art()
    s = A.man["characters"]
    anims = s["animations"]
    looks = s["variants"]["count"]
    fw, fh = s["frameWidth"], s["frameHeight"]
    _, img = A.sheet("characters")
    blocks = []
    for n, a in anims.items():
        fr = []
        for f in a["frames"]:
            if f not in fr:
                fr.append(f)
        blocks.append((n, fr))
    maxf = max(len(fr) for _, fr in blocks)
    lab_w = 124
    group_w = maxf * (fw + 1) + 6
    W = lab_w + looks * group_w
    H = len(blocks) * (fh + 3) + 4
    sheet = Image.new("RGBA", (W, H), (58, 46, 56, 255))
    for bi, (n, fr) in enumerate(blocks):
        oy = 2 + bi * (fh + 3)
        if bi % 2 == 0:
            sheet.paste((66, 53, 63, 255), (0, oy - 1, W, oy + fh + 2))
        A.text(sheet, 3, oy + fh // 2 - 3, n, TEXT)
        for lk in range(looks):
            ox = lab_w + lk * group_w
            for j, f in enumerate(fr):
                cell = img.crop((f * fw, lk * fh, f * fw + fw, lk * fh + fh))
                paste(sheet, cell, ox + j * (fw + 1), oy)
    sheet.resize((W * scale, H * scale), Image.NEAREST).save(out)
