# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds every scene sprite and writes manifest.json + layout.json."""

import json

from . import props as P
from . import fx, room, ui, characters
from .core import C, MANIFEST, OUT, Sheet, single, mirror, new, rect, alpha, hexc
from .props import CLUTTER, SCREEN_KINDS

LAYERS = ["background", "floor", "wall", "light_floor", "shadow", "objects", "light", "ambient", "ui"]


def sheet(name, frames, anchor=(0, 0), names=None, anims=None, **extra):
    f0 = frames[0]
    s = Sheet(name, f0.width, f0.height, anchor=anchor)
    for i, f in enumerate(frames):
        s.add(f, names[i] if names else None)
    for k, v in (anims or {}).items():
        s.anim(k, *v)
    s.extra.update(extra)
    s.save()
    return s


def build_sprites():
    single("room", room.build())
    sheet("desk", [P.desk("oak", 3), P.desk("walnut", 4), P.desk("oak", 8)], names=["oak", "walnut", "oak_b"])
    chairs = ["ink", "teal", "red", "blue"]
    sheet("chair_back", [P.chair(c, "up") for c in chairs], anchor=(8, 13), names=chairs)
    sheet("chair_front", [P.chair(c, "down") for c in chairs], anchor=(8, 21), names=chairs)
    single("lead_desk", P.lead_desk())
    single("lead_chair", P.lead_chair(), anchor=(11, 25))
    mf = [P.monitor_front(k, i) for i, k in enumerate(SCREEN_KINDS)]
    sheet("monitor_front", mf, anchor=(10, 17), names=SCREEN_KINDS, screen=list(P.MON_SCREEN),
          screenKinds={"diff": ["diff"], "code": ["code_a", "code_b"], "terminal": ["terminal_a", "terminal_b"],
                       "doc": ["doc"], "search": ["search"], "web": ["web"], "brief": ["brief"],
                       "chat": ["chat"], "plan": ["plan"], "question": ["question"], "tool": ["code_b", "code_a"],
                       "error": ["error"], "off": ["off"]})
    sheet("monitor_back", [P.monitor_back(True), P.monitor_back(False)], anchor=(10, 17), names=["on", "off"])
    bez, glass, srect = P.big_bezel()
    single("monitor_big", bez, screen=list(srect))
    single("monitor_big_glass", glass)
    P.clutter_sheet()
    sheet("server_rack", [P.server_rack(i) for i in range(4)], anims={"blink": ([0, 1, 2, 3], 3, True)})
    sheet("net_cabinet", [P.net_cabinet(i) for i in range(4)], anims={"blink": ([0, 1, 2, 3], 4, True)})
    single("counter", P.counter())
    single("upper_cabinets", P.upper_cabinets())
    sheet("coffee_machine", [P.coffee_machine(i) for i in range(4)], names=["idle", "brew1", "brew2", "brew3"],
          anims={"idle": ([0], 1, True), "brew": ([1, 2, 3, 2, 3], 4, True)})
    sheet("steam", [P.steam(i) for i in range(6)], anchor=(5, 15), anims={"rise": (list(range(6)), 6, True)})
    single("fridge", P.fridge())
    sheet("water_cooler", [P.water_cooler(i) for i in range(3)], anims={"bubble": ([0, 0, 0, 0, 1, 2], 4, True)})
    single("sofa", P.sofa())
    arm = P.armchair()
    arm_front = new(arm.width, arm.height)
    arm_front.alpha_composite(arm.crop((0, 17, arm.width, 25)), (0, 17))
    sheet("armchair", [arm, mirror(arm), arm_front, mirror(arm_front)],
          names=["right", "left", "right_front", "left_front"])
    single("coffee_table", P.coffee_table())
    sheet("floor_lamp", [P.floor_lamp(True), P.floor_lamp(False)], names=["on", "off"])
    single("bookshelf", P.bookshelf())
    sheet("record_player", [P.record_player(i) for i in range(4)], anims={"spin": ([0, 1, 2, 3], 6, True)})
    sheet("plant_monstera", [P.monstera(0), P.monstera(1)], anims={"sway": ([0, 0, 1, 1], 1, True)})
    single("plant_snake", P.snake_plant())
    sheet("plant_ficus", [P.ficus(0), P.ficus(1)], anims={"sway": ([0, 0, 1, 1], 1, True)})
    sheet("plant_hanging", [P.hanging_plant(0), P.hanging_plant(1.2)], anims={"sway": ([0, 1], 1, True)})
    single("plant_cactus", P.cactus_big())
    single("window_frame", P.window_frame(), glass=list(P.WINDOW_GLASS))
    sheet("window_sky", [P.window_sky(m) for m in ("dawn", "day", "dusk", "night")], names=["dawn", "day", "dusk", "night"])
    sheet("window_beam", [P.window_beam(m) for m in ("dawn", "day", "dusk", "night")], names=["dawn", "day", "dusk", "night"])
    single("clock_face", P.clock_face(), anchor=(9, 9))
    sheet("clock_hour", [P.clock_hand(k, 3, C["ink"][1]) for k in range(12)], anchor=(9, 9))
    sheet("clock_minute", [P.clock_hand(k, 5, C["ink"][2]) for k in range(12)], anchor=(9, 9))
    single("kanban", P.kanban())
    single("painting", P.painting())
    single("poster", P.poster())
    single("wall_shelf", P.wall_shelf())
    sheet("julius_door", [P.julius_door(i) for i in range(3)], names=["closed", "ajar", "open"])
    sheet("door_lamp", [P.door_lamp(i) for i in range(3)], names=["off", "busy", "free"])
    single("exit_sign", P.exit_sign())
    sheet("printer", [P.printer(i) for i in range(4)], names=["idle", "p1", "p2", "p3"],
          anims={"idle": ([0], 1, True), "print": ([1, 2, 3, 0], 4, True)})
    single("whiteboard", P.whiteboard_stand())
    single("coat_rack", P.coat_rack())
    single("umbrella_stand", P.umbrella_stand())
    single("trash_bin", P.trash_bin())
    single("cat_bed", P.cat_bed())
    single("mat_julius", P.mat(32, 9, "red", "stripes"))
    single("mat_entrance", P.mat(30, 12, "sage", "dots"))
    single("rug_lounge", P.rug(152, 78, "blue", "blue", "yellow", 1))
    single("rug_lead", P.rug(96, 40, "red", "red", "cream", 2))
    single("carpet_work", P.carpet(214, 82))
    P.cat_sheet()
    # overlays
    single("shadow_person", fx.shadow(16, 5), anchor=(8, 3))
    single("shadow_cat", fx.shadow(14, 4), anchor=(7, 2))
    sheet("monitor_glow", [fx.monitor_glow(fx.GLOW[k]) for k in fx.GLOW_ORDER], anchor=(22, 11), names=fx.GLOW_ORDER,
          kindGlow={"code": "cool", "diff": "cool", "terminal": "green", "doc": "warm", "web": "warm", "plan": "warm",
                    "search": "cool", "chat": "cool", "brief": "cool", "tool": "cool", "error": "red", "question": "violet"})
    single("pool_warm", fx.pool(96, 56, hexc("#ffd890")), anchor=(48, 28))
    single("pool_small", fx.pool(40, 22, hexc("#ffe0a0"), 0.9), anchor=(20, 11))
    single("halo_warm", fx.banded(26, 20, hexc("#ffe6a8"), [30, 50, 80]), anchor=(13, 10))
    single("strip_glow", fx.banded(84, 26, hexc("#fff0c0"), [14, 22, 32], cy=4), anchor=(0, 0))
    sheet("rack_glow", [fx.banded(46, 70, hexc("#7fe0d0"), [8, 14, 20]), fx.banded(46, 70, hexc("#7fe0d0"), [10, 18, 26])],
          anims={"pulse": ([0, 1], 1, True)})
    sheet("door_glow", [fx.banded(30, 22, hexc("#ff5a5a"), [18, 32, 50]), fx.banded(30, 22, hexc("#6af0a0"), [18, 32, 50])],
          anchor=(15, 11), names=["busy", "free"])
    single("door_spill", fx.banded(48, 26, hexc("#ffd890"), [18, 30, 44], cy=2), anchor=(24, 0))
    sheet("dust", [fx.dust(i) for i in range(4)], anchor=(1, 1), anims={"twinkle": ([0, 1, 2, 1], 3, True)})
    single("vignette", fx.vignette(480, 270))
    # shadows under furniture
    for name, (w, h) in {"sh_desk_pod": (86, 14), "sh_lead_desk": (78, 12), "sh_rack": (32, 8), "sh_counter": (76, 8),
                         "sh_fridge": (28, 7), "sh_sofa": (66, 10), "sh_chair": (18, 6), "sh_small": (20, 6),
                         "sh_medium": (32, 8), "sh_plant": (22, 6), "sh_table": (40, 10), "sh_shelf": (40, 8)}.items():
        single(name, fx.shadow(w, h) if name not in ("sh_desk_pod", "sh_lead_desk", "sh_counter")
               else fx.rect_shadow(w, h), anchor=(w // 2, h // 2))
    characters.build()
    ui.build()


# ------------------------------------------------------------------ layout --

class L:
    def __init__(self):
        self.props = []
        self.lights = []
        self.hotspots = []
        self.nodes = {}
        self.edges = []

    def prop(self, id, sprite, x, y, layer="objects", z=None, **kw):
        d = {"id": id, "sprite": sprite, "x": x, "y": y, "layer": layer}
        if layer == "objects":
            assert z is not None, id
        if z is not None:
            d["z"] = z
        d.update(kw)
        self.props.append(d)
        return d

    def shadow(self, id, sprite, x, y):
        return self.prop(id + "_shadow", sprite, x, y, layer="shadow")

    def node(self, id, x, y):
        self.nodes[id] = (x, y)
        return id

    def edge(self, *ids):
        for a, b in zip(ids, ids[1:]):
            self.edges.append([a, b])


def build_layout():
    lay = L()
    M = MANIFEST

    def size(s):
        return M[s]["frameWidth"], M[s]["frameHeight"]

    lay.prop("room", "room", 0, 0, layer="background")
    # ----- floor decals
    lay.prop("rug_lead", "rug_lead", 197, 70, layer="floor")
    lay.prop("rug_lounge", "rug_lounge", 314, 176, layer="floor")
    lay.prop("carpet_work", "carpet_work", 12, 132, layer="floor")
    lay.prop("mat_julius", "mat_julius", 393, 60, layer="floor")
    lay.prop("mat_entrance", "mat_entrance", 102, 248, layer="floor")

    # ----- wall
    lay.prop("upper_cabinets", "upper_cabinets", 60, 8, layer="wall")
    lay.prop("poster", "poster", 160, 12, layer="wall")
    for i, wx in enumerate((184, 284)):
        g = P.WINDOW_GLASS
        lay.prop("window%d_sky" % i, "window_sky", wx + g[0], 10 + g[1], layer="wall", frame="day",
                 timeOfDay=True)
        lay.prop("window%d" % i, "window_frame", wx, 10, layer="wall")
    lay.prop("clock", "clock_face", 260, 22, layer="wall", clock={"hour": "clock_hour", "minute": "clock_minute"})
    lay.prop("kanban", "kanban", 340, 12, layer="wall")
    lay.prop("julius_door", "julius_door", 392, 4, layer="wall", frame="closed")
    lay.prop("door_lamp", "door_lamp", 428, 8, layer="wall", frame="free")
    lay.prop("painting", "painting", 438, 10, layer="wall")
    lay.prop("wall_shelf", "wall_shelf", 434, 34, layer="wall")
    lay.prop("plant_hanging", "plant_hanging", 150, 2, layer="wall", animation="sway")

    # ----- server corner
    lay.shadow("rack", "sh_rack", 26, 86)
    lay.prop("server_rack", "server_rack", 10, 26, z=86, animation="blink")
    lay.shadow("netcab", "sh_small", 51, 86)
    lay.prop("net_cabinet", "net_cabinet", 41, 56, z=86, animation="blink")
    # ----- kitchen
    lay.shadow("counter", "sh_counter", 97, 83)
    lay.prop("counter", "counter", 60, 52, z=82)
    lay.prop("coffee_machine", "coffee_machine", 64, 34, z=82.2, animation="idle")
    lay.prop("coffee_steam", "steam", 72, 44, layer="ambient", animation="rise")
    lay.prop("counter_mug", "clutter", 90, 58, z=82.3, frame="mug_white")
    lay.prop("counter_mug2", "clutter", 120, 58, z=82.3, frame="plant_pot")
    lay.prop("counter_snack", "clutter", 112, 58, z=82.3, frame="snack")
    lay.shadow("fridge", "sh_fridge", 148, 85)
    lay.prop("fridge", "fridge", 136, 30, z=84)
    lay.shadow("cooler", "sh_small", 169, 85)
    lay.prop("water_cooler", "water_cooler", 162, 52, z=84, animation="bubble")
    lay.prop("trash_kitchen", "trash_bin", 177, 74, z=86)

    # ----- lead desk
    lx, ly = 209, 80
    lay.shadow("lead_desk", "sh_lead_desk", lx + 38, ly + 30)
    lay.prop("lead_chair", "lead_chair", lx + 36, 88, z=87)  # anchor = floor point
    lay.prop("lead_desk_top", "lead_desk", lx, ly, z=ly + 20)
    lead_mons = []
    for i, mx in enumerate((lx + 15, lx + 57)):
        lead_mons.append(lay.prop("lead_monitor%d" % i, "monitor_back", mx, ly + 16, z=ly + 20.5, frame="on"))
    for (cx, cy, fr) in ((lx + 36, ly + 18, "nameplate"), (lx + 4, ly + 12, "lamp_on"), (lx + 66, ly + 14, "papers"),
                         (lx + 27, ly + 13, "mug_steam"), (lx + 47, ly + 14, "phone")):
        lay.prop("lead_" + fr, "clutter", cx, cy, z=ly + 20.6, frame=fr)
    lay.prop("plant_lead", "plant_snake", 290, 60, z=87)
    lay.shadow("plant_lead", "sh_plant", 298, 87)

    # ----- pods: 8 desks
    desks = []
    P0 = 152
    pods = (("A", 24), ("B", 136))
    chair_cols = ["teal", "ink", "red", "blue", "ink", "red", "blue", "teal"]
    desk_frames = ["oak", "oak", "oak", "oak", "walnut", "walnut", "walnut", "walnut"]
    clutters = [
        [("cactus", 4, 8), ("mug_red", 33, 10), ("sticky", 28, 4)],
        [("books", 33, 8), ("figurine_robot", 5, 9), ("coffee_cup", 27, 12)],
        [("keyboard", 20, 15), ("mouse", 29, 14), ("lamp_on", 4, 12), ("papers_messy", 34, 15)],
        [("keyboard_rgb", 20, 15), ("mouse", 29, 14), ("headphones", 34, 13), ("can", 4, 15), ("duck", 8, 9)],
        [("succulent", 33, 9), ("notebook", 5, 11), ("photo", 28, 6)],
        [("pencil_cup", 4, 8), ("mug_white", 33, 11), ("cube", 26, 4)],
        [("keyboard", 20, 15), ("mouse", 29, 14), ("book_open", 6, 15), ("mug_steam", 34, 9), ("figurine_dino", 34, 15)],
        [("keyboard", 20, 15), ("mouse", 10, 14), ("plant_pot", 33, 14), ("sticky", 5, 9), ("glasses_case", 30, 8)],
    ]
    for pi, (pod, px0) in enumerate(pods):
        lay.shadow("pod" + pod, "sh_desk_pod", px0 + 42, P0 + 36)
        for k in range(4):
            idx = pi * 4 + k
            col = k % 2
            top = k < 2
            dx = px0 + col * 40
            cx = dx + 20
            if top:
                dy = P0
                seat = (cx, P0 + 8)
                facing = "down"
                chair = ("chair_front", cx, seat[1], seat[1] - 1)
                mon = ("monitor_back", cx, P0 + 14, P0 + 17.5)
                dz = P0 + 17
            else:
                dy = P0 + 16
                seat = (cx, P0 + 42)
                facing = "up"
                chair = ("chair_back", cx, seat[1], seat[1] + 1)
                mon = ("monitor_front", cx, P0 + 18, P0 + 33.5)
                dz = P0 + 33
            did = "desk%d" % idx
            dsk = lay.prop(did, "desk", dx, dy, z=dz, frame=desk_frames[idx])
            ch = lay.prop(did + "_chair", chair[0], chair[1], chair[2], z=chair[3], frame=chair_cols[idx])
            mw, mh = size(mon[0])
            mn = lay.prop(did + "_monitor", mon[0], mon[1], mon[2], z=mon[3], frame="off" if top is False else "on")
            mrect = [mon[1] - 10, mon[2] - 17, mw, mh]
            screen = None
            if not top:
                s = P.MON_SCREEN
                screen = [mrect[0] + s[0], mrect[1] + s[1], s[2], s[3]]
            cl = []
            for j, (fr, ox, oy) in enumerate(clutters[idx]):
                cl.append(lay.prop("%s_clutter%d" % (did, j), "clutter", dx + ox, dy + oy, z=dz + 0.2 + j * 0.01, frame=fr))
                if fr == "lamp_on":
                    lay.lights.append({"id": did + "_lamp", "sprite": "pool_small", "x": dx + ox + 3, "y": dy + oy - 4,
                                       "layer": "light", "kind": "lamp", "day": 0.5, "night": 1.0})
            glow = {"sprite": "monitor_glow", "x": mon[1], "y": mon[2] - 10, "layer": "light"}
            ap = "d%d_in" % idx
            lay.node(ap, cx, 120 if top else 212)
            sid = "d%d_seat" % idx
            lay.node(sid, seat[0], seat[1])
            lay.edge(ap, sid)
            desks.append({
                "index": idx, "pod": pod, "facing": facing,
                "desk": did, "chair": did + "_chair", "monitor": did + "_monitor",
                "clutter": [c["id"] for c in cl],
                "seat": {"x": seat[0], "y": seat[1], "z": seat[1] + (0 if top else 0.5)},
                "seatNode": sid, "approach": ap,
                "monitorRect": mrect, "screenRect": screen,
                "glow": glow,
                "label": {"x": cx, "y": seat[1] - 34},
                "bubble": {"x": cx + 9, "y": seat[1] - 28} if top else {"x": cx + 13, "y": seat[1] - 16},
            })
            lay.hotspots.append({"id": "monitor%d" % idx, "kind": "monitor", "desk": idx, "rect": mrect})

    # ----- lead record
    lead_rect = [lx + 15 - 10, ly - 1, 62, 18]
    lay.node("lead_seat", lx + 36, 88)
    lay.node("lead_behind", 200, 92)
    lay.edge("lead_behind", "lead_seat")
    lead = {
        "desk": "lead_desk_top", "chair": "lead_chair", "monitors": ["lead_monitor0", "lead_monitor1"],
        "facing": "down", "seat": {"x": lx + 36, "y": 88, "z": 88}, "seatNode": "lead_seat",
        "approach": "lead_behind", "monitorRect": lead_rect, "screenRect": None,
        "glow": {"sprite": "monitor_glow", "x": lx + 36, "y": ly + 2, "layer": "light"},
        "label": {"x": lx + 36, "y": 88 - 34}, "bubble": {"x": lx + 45, "y": 88 - 28},
        "visitorSpots": ["lead_visit0", "lead_visit1"],
    }
    lay.hotspots.append({"id": "lead_monitor", "kind": "monitor", "desk": -1, "rect": lead_rect})

    # ----- whiteboard, printer, entrance, plants
    lay.shadow("whiteboard", "sh_shelf", 272, 190)
    lay.prop("whiteboard", "whiteboard", 250, 146, z=189)
    lay.shadow("printer", "sh_medium", 214, 246)
    lay.prop("printer", "printer", 200, 216, z=246, animation="idle")
    lay.prop("trash_printer", "trash_bin", 232, 236, z=246)
    lay.shadow("ficus", "sh_plant", 21, 259)
    lay.prop("plant_ficus", "plant_ficus", 9, 214, z=259, animation="sway")
    lay.prop("coat_rack", "coat_rack", 78, 216, z=259)
    lay.shadow("coat", "sh_small", 87, 259)
    lay.prop("umbrella_stand", "umbrella_stand", 140, 242, z=259)
    lay.prop("plant_cactus", "plant_cactus", 446, 62, z=85)
    lay.shadow("cactus", "sh_small", 453, 85)
    lay.prop("trash_pods", "trash_bin", 236, 128, z=139)

    # ----- lounge
    lay.shadow("sofa", "sh_sofa", 391, 199)
    lay.prop("sofa", "sofa", 361, 168, z=190)
    lay.shadow("table", "sh_table", 392, 223)
    lay.prop("coffee_table", "coffee_table", 374, 206, z=223)
    lay.shadow("arm_l", "sh_medium", 336, 226)
    lay.prop("armchair_l", "armchair", 322, 197, z=221, frame="right")
    lay.prop("armchair_l_front", "armchair", 322, 197, z=222.5, frame="right_front")
    lay.shadow("arm_r", "sh_medium", 449, 226)
    lay.prop("armchair_r", "armchair", 436, 197, z=221, frame="left")
    lay.prop("armchair_r_front", "armchair", 436, 197, z=222.5, frame="left_front")
    lay.shadow("lamp", "sh_small", 351, 199)
    lay.prop("floor_lamp", "floor_lamp", 344, 156, z=199, frame="on")
    lay.prop("record_player", "record_player", 426, 178, z=199, animation="spin")
    lay.shadow("bookshelf", "sh_shelf", 454, 168)
    lay.prop("bookshelf", "bookshelf", 436, 118, z=167)
    lay.shadow("monstera", "sh_plant", 314, 260)
    lay.prop("plant_monstera", "plant_monstera", 300, 222, z=260, animation="sway")
    lay.shadow("catbed", "sh_small", 431, 256)
    lay.prop("cat_bed", "cat_bed", 420, 245, z=250)
    lay.prop("plant_snake2", "plant_snake", 456, 232, layer="objects", z=259)

    # ----- window sills
    lay.prop("sill_succulent", "clutter", 194, 46, layer="wall", frame="succulent")
    lay.prop("sill_cactus", "clutter", 326, 46, layer="wall", frame="cactus")

    # ----- lights
    for i, wx in enumerate((184, 284)):
        lay.lights.append({"id": "beam%d" % i, "sprite": "window_beam", "x": wx + 2, "y": 60, "layer": "light_floor",
                           "kind": "window", "timeOfDay": True, "dust": [wx + 10, 60, 90, 100]})
    lay.lights += [
        {"id": "lounge_lamp", "sprite": "pool_warm", "x": 351, "y": 200, "layer": "light", "kind": "lamp", "day": 0.6, "night": 1.0},
        {"id": "lounge_lamp_halo", "sprite": "halo_warm", "x": 351, "y": 162, "layer": "light", "kind": "lamp", "day": 0.6, "night": 1.0},
        {"id": "lead_lamp", "sprite": "pool_small", "x": lx + 8, "y": ly + 8, "layer": "light", "kind": "lamp", "day": 0.5, "night": 1.0},
        {"id": "kitchen_strip", "sprite": "strip_glow", "x": 56, "y": 28, "layer": "light", "kind": "strip", "day": 0.6, "night": 1.0},
        {"id": "rack_glow", "sprite": "rack_glow", "x": 1, "y": 20, "layer": "light", "kind": "led", "animation": "pulse", "day": 0.6, "night": 1.0},
        {"id": "door_glow", "sprite": "door_glow", "x": 432, "y": 11, "layer": "light", "kind": "status", "frame": "free", "day": 0.8, "night": 1.0},
        {"id": "door_spill", "sprite": "door_spill", "x": 409, "y": 60, "layer": "light_floor", "kind": "door", "when": "julius_door!=closed", "day": 1.0, "night": 1.0},
        {"id": "vignette", "sprite": "vignette", "x": 0, "y": 0, "layer": "ambient", "kind": "vignette", "day": 0.6, "night": 1.0},
    ]

    # ----- hotspots
    lay.hotspots += [
        {"id": "server_rack", "kind": "rack", "rect": [8, 24, 56, 64]},
        {"id": "coffee_machine", "kind": "coffee", "rect": [62, 32, 22, 26]},
        {"id": "julius_door", "kind": "door", "rect": [390, 2, 38, 60]},
        {"id": "cat", "kind": "cat", "rect": [-9, -13, 18, 14], "follows": "cat"},
    ]

    # ----- walk graph
    n = lay.node
    for x in (14, 44, 84, 120, 156, 196, 228, 300):
        n("c_%d" % x, x, 120)
    for x in (14, 44, 84, 120, 156, 196, 228, 268, 306):
        n("b_%d" % x, x, 212)
    tops = [14, 44, 84, 120, 156, 196, 228, 300]
    lay.edge(*["c_%d" % x for x in tops])
    bots = [14, 44, 84, 120, 156, 196, 228, 268, 306]
    lay.edge(*["b_%d" % x for x in bots])
    for x in (14, 120, 228):
        lay.edge("c_%d" % x, "b_%d" % x)
    # desk approach nodes sit on the corridors
    for d in desks:
        ap = d["approach"]
        ax, ay = lay.nodes[ap]
        row = "c" if ay == 120 else "b"
        # nearest corridor neighbours
        lay.edge(ap, "%s_%d" % (row, ax))
    n("server", 34, 98)
    lay.edge("server", "c_14")
    lay.edge("server", "c_44")
    n("coffee", 73, 94)
    lay.edge("coffee", "c_84")
    lay.edge("coffee", "c_44")
    n("fridge", 148, 100)
    lay.edge("fridge", "c_120")
    lay.edge("fridge", "c_156")
    n("cooler", 169, 98)
    lay.edge("cooler", "c_156")
    lay.edge("cooler", "c_196")
    lay.edge("lead_behind", "c_196")
    n("lead_visit0", 232, 124)
    n("lead_visit1", 260, 124)
    lay.edge("c_228", "lead_visit0", "lead_visit1", "c_300")
    n("window", 296, 76)
    lay.edge("window", "c_300")
    n("hall", 346, 104)
    lay.edge("c_300", "hall")
    n("kanban", 366, 76)
    lay.edge("kanban", "hall")
    for i, (x, y) in enumerate(((409, 74), (397, 87), (385, 100), (373, 113))):
        n("queue%d" % i, x, y)
    lay.edge("queue3", "queue2", "queue1", "queue0")
    lay.edge("hall", "queue3")
    n("julius", 409, 62)
    lay.edge("queue0", "julius")
    n("whiteboard", 272, 202)
    lay.edge("whiteboard", "b_268")
    n("lounge_in", 306, 160)
    lay.edge("c_300", "lounge_in", "b_306")
    n("lounge_w", 318, 240)
    lay.edge("b_306", "lounge_w")
    n("sofa_front", 356, 206)
    lay.edge("b_306", "sofa_front")
    n("lounge_s", 392, 246)
    lay.edge("lounge_w", "lounge_s")
    n("lounge_e", 462, 214)
    lay.edge("lounge_s", "lounge_e")
    n("bookshelf_spot", 454, 186)
    lay.edge("lounge_e", "bookshelf_spot")
    n("entry_in", 117, 228)
    n("entrance", 117, 256)
    lay.edge("b_120", "entry_in", "entrance")
    n("printer_spot", 214, 254)
    n("printer_way", 176, 236)
    lay.edge("entry_in", "printer_way", "printer_spot")
    lay.edge("printer_way", "b_196")
    lay.edge("printer_way", "b_228")
    # lounge seats
    lounge = []
    for i, sx in enumerate((374, 392, 410)):
        sid = "sofa%d" % i
        n(sid, sx, 196)
        lay.edge("sofa_front", sid) if i == 0 else lay.edge("sofa%d" % (i - 1), sid)
        lounge.append({"id": sid, "x": sx, "y": 196, "z": 196, "facing": "down", "node": sid,
                       "poses": ["lounge_relax_down", "lounge_sip_down", "lounge_sleep_down"]})
    n("armchair_l", 336, 222)
    lay.edge("lounge_w", "armchair_l")
    lay.edge("b_306", "armchair_l")
    lounge.append({"id": "armchair_l", "x": 336, "y": 222, "z": 222, "facing": "right", "node": "armchair_l",
                   "front": "armchair_l_front",
                   "poses": ["lounge_relax_right", "lounge_sip_right", "lounge_sleep_right"]})
    n("armchair_r", 448, 222)
    lay.edge("lounge_e", "armchair_r")
    lounge.append({"id": "armchair_r", "x": 448, "y": 222, "z": 222, "facing": "left", "node": "armchair_r",
                   "front": "armchair_r_front",
                   "poses": ["lounge_relax_left", "lounge_sip_left", "lounge_sleep_left"]})
    # cat-only spots
    n("cat_bed", 431, 252)
    lay.edge("lounge_e", "cat_bed")
    lay.edge("lounge_s", "cat_bed")
    n("cat_sun", 236, 150)
    lay.edge("cat_sun", "c_228")
    n("cat_rug", 392, 234)
    lay.edge("cat_rug", "lounge_s")

    standing = [
        {"id": "coffee", "node": "coffee", "facing": "up", "activity": "coffee", "pose": "idle_up", "then": "drink"},
        {"id": "cooler", "node": "cooler", "facing": "up", "activity": "water", "pose": "idle_up", "then": "drink"},
        {"id": "fridge", "node": "fridge", "facing": "up", "activity": "snack", "pose": "idle_up"},
        {"id": "server", "node": "server", "facing": "up", "activity": "server", "pose": "idle_up"},
        {"id": "whiteboard", "node": "whiteboard", "facing": "up", "activity": "planning", "pose": "talk_up"},
        {"id": "kanban", "node": "kanban", "facing": "up", "activity": "planning", "pose": "talk_up"},
        {"id": "window", "node": "window", "facing": "up", "activity": "break", "pose": "idle_up", "then": "stretch"},
        {"id": "printer", "node": "printer_spot", "facing": "up", "activity": "printing", "pose": "idle_up"},
        {"id": "bookshelf", "node": "bookshelf_spot", "facing": "up", "activity": "reading", "pose": "idle_up"},
        {"id": "lead_visit0", "node": "lead_visit0", "facing": "up", "activity": "talking", "pose": "talk_up"},
        {"id": "lead_visit1", "node": "lead_visit1", "facing": "up", "activity": "talking", "pose": "talk_up"},
    ]
    for s in standing:
        s["x"], s["y"] = lay.nodes[s["node"]]
    queue = [{"id": "queue%d" % i, "node": "queue%d" % i, "x": lay.nodes["queue%d" % i][0],
              "y": lay.nodes["queue%d" % i][1], "facing": "up"} for i in range(4)]

    nodes = [{"id": k, "x": v[0], "y": v[1]} for k, v in lay.nodes.items()]
    # sanity: every node is connected
    seen = set()
    adj = {}
    for a, b in lay.edges:
        assert a in lay.nodes and b in lay.nodes, (a, b)
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    stack = ["entrance"]
    while stack:
        k = stack.pop()
        if k in seen:
            continue
        seen.add(k)
        stack += adj.get(k, [])
    missing = set(lay.nodes) - seen
    assert not missing, missing

    return {
        "version": 2,
        "size": [room.W, room.H],
        "scale": {"preferred": 3, "minimum": 2},
        "layers": LAYERS,
        "font": {"file": "../fonts/monogram-extended.ttf", "family": "monogram", "pixelSize": 16,
                 "lineHeight": 10, "capHeight": 7, "advance": 6},
        "props": lay.props,
        "desks": desks,
        "lead": lead,
        "lounge": lounge,
        "standing": standing,
        "queue": queue,
        "entrance": {"node": "entrance", "x": 117, "y": 256, "offscreen": {"x": 117, "y": 300}},
        "julius": {"door": "julius_door", "lamp": "door_lamp", "glow": "door_glow", "node": "julius"},
        "cat": {"sprite": "cat", "shadow": "shadow_cat", "home": "cat_bed",
                "spots": [{"node": "cat_bed", "pose": "sleep"}, {"node": "cat_sun", "pose": "sleep"},
                          {"node": "cat_rug", "pose": "sit"}, {"node": "coffee", "pose": "sit"},
                          {"node": "server", "pose": "sit"}, {"node": "whiteboard", "pose": "sit"}]},
        "graph": {"nodes": nodes, "edges": lay.edges},
        "lights": lay.lights,
        "hotspots": lay.hotspots,
        "characters": {"sprite": "characters", "shadow": "shadow_person",
                       "labelOffset": [0, -34], "bubbleOffset": [9, -30], "walkSpeed": 36},
    }


def write():
    build_sprites()
    layout = build_layout()
    (OUT / "layout.json").write_text(json.dumps(layout, indent=1) + "\n")
    man = {"version": 2, "sheets": MANIFEST}
    (OUT / "manifest.json").write_text(json.dumps(man, indent=1) + "\n")
    return man, layout
