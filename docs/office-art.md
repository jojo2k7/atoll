# Office art: file formats

Everything the office draws comes from `data/office/` and `data/fonts/`. Two
JSON files describe it:

- `data/office/manifest.json`: every sprite sheet (file, frame grid, anchor,
  named frames, animations, 9-slice borders, extra metadata).
- `data/office/layout.json`: the scene (size, layers, every placed prop, desks,
  seats, spots, walk graph, lights, hotspots).

Both are generated, together with every PNG, by
`scripts/gen-office-sprites.py` (Python 3 + Pillow, deterministic; the code is
in `scripts/office_art/`). Do not edit the JSON by hand; change the generator
and re-run it. QML must not hard-code any coordinate that these files provide.

```
python3 scripts/gen-office-sprites.py                       # PNGs + JSON
python3 scripts/gen-office-sprites.py --preview out.png \
        --characters chars.png                              # + reference renders
```

The preview (`scripts/office_art/preview.py`) composes the scene purely from
the two JSON files, exactly as described below. When QML and the preview
disagree, the preview is the reference.

## 1. Conventions

- **Units**: logical pixels. Origin top-left, x to the right, y down.
- **Scene size**: `layout.size` = `[480, 270]`. Render it at an integer scale
  (`layout.scale.preferred` 3 on 1920-wide screens, never below
  `layout.scale.minimum` 2), with `smooth: false` everywhere. Never place
  anything at a fractional logical position; round to integers before scaling.
- **Light** comes from the top left. Do not mirror props (characters and the
  cat have explicit left/right frames).
- **Anchor**: every sheet has `anchor: [ax, ay]` in frame pixels. A sprite
  placed at `(x, y)` is drawn with its frame's top-left at `(x - ax, y - ay)`.
  Characters and the cat: anchor = floor point under the feet (or under the
  seat). Most props: anchor `[0, 0]`, so `(x, y)` is the top-left corner.
- **Opacity**: overlay sprites are already semi-transparent; draw them with
  normal alpha blending. `day` / `night` values in `lights` are extra opacity
  multipliers.

## 2. manifest.json

```json
{ "version": 2,
  "sheets": { "<name>": <Sheet>, ... } }
```

`Sheet`:

| field | type | meaning |
|-------|------|---------|
| `file` | string | PNG next to the manifest |
| `frameWidth`, `frameHeight` | int | size of one frame |
| `columns` | int | frames per row in the PNG |
| `frameCount` | int | number of frames in the PNG (for `variants` sheets: all rows together) |
| `anchor` | [int, int] | see section 1 |
| `kind` | `"sprite"` \| `"character"` | informational |
| `frameNames` | {name: index} | optional; frame names usable wherever a frame is expected |
| `animations` | {name: {frames: [int], fps: number, loop: bool}} | optional |
| `variants` | {key, count, rule} | only on `characters`: one PNG row per variant |
| `slice` | [left, top, right, bottom] | optional; 9-slice borders in pixels |
| others | | sheet-specific metadata, listed in section 6 |

**Frame lookup.** Frame index `i` of a normal sheet is at
`x = (i % columns) * frameWidth`, `y = floor(i / columns) * frameHeight`.
For a sheet with `variants` (only `characters`), the variant (the agent's
`look`, 0-7) selects the row and the frame index is the column:
`x = i * frameWidth`, `y = look * frameHeight`.

A frame reference anywhere in `layout.json` is either an int index or a name
from `frameNames`. No frame and no animation means frame 0.

**Animations.** `frames` lists frame indices (repeats are intentional, they
are the timing). Show `frames[k]` with `k = floor(t * fps)`; when `loop` is
false, hold the last frame. Give every agent its own random phase offset so
nobody breathes in sync.

**9-slice.** `slice` = border widths. Corners are drawn 1:1, edges are
stretched in one direction and the centre in both, all nearest-neighbour
(`BorderImage` with `horizontalTileMode/verticalTileMode: BorderImage.Stretch`
and `smooth: false`). Sizes must stay integers.

## 3. layout.json

```
{ version, size, scale, layers, font, props, desks, lead, lounge, standing,
  queue, entrance, julius, cat, graph, lights, hotspots, characters }
```

### 3.1 layers and z-sorting

`layers` = draw order, back to front:

| layer | content | sorted |
|-------|---------|--------|
| `background` | `room` (floor, walls, trims baked into one image) | no |
| `floor` | rugs, carpet, mats | file order |
| `wall` | wall-mounted things: windows (sky first, then frame), clock, posters, doors, shelves | file order |
| `light_floor` | light falling on the floor: window beams, door spill | file order |
| `shadow` | drop shadows of props, then of every character and the cat | file order |
| `objects` | furniture, monitors, clutter, characters, the cat | **by z** |
| `light` | glows and light pools drawn over everything | file order |
| `ambient` | steam, dust motes, vignette | file order |
| `ui` | name tags, emote bubbles, speech bubbles, in-scene panels | file order |

**Z rule (objects layer).** Every object has a number `z` (float). Draw in
ascending `z`. Ties: props before characters, then file order. For props `z`
is given explicitly in the layout. For a standing or walking character
`z = y` of its anchor (its feet). For a seated character use the seat's `z`
(`desks[i].seat.z`, `lead.seat.z`, `lounge[i].z`). The cat: `z = y`.
Consequences that are already baked into the numbers: a worker facing the
camera sits in front of their chair and behind their desk; a worker seen from
behind sits in front of their desk and behind the low chair back; a lounge
sitter in an armchair is covered by the armchair's front piece
(`lounge[i].front`).

### 3.2 props

```json
{ "id": "desk0", "sprite": "desk", "x": 24, "y": 152, "layer": "objects",
  "z": 169, "frame": "oak", "animation": "blink" }
```

| field | meaning |
|-------|---------|
| `id` | unique; other sections refer to props by id |
| `sprite` | manifest sheet name |
| `x`, `y` | anchor position |
| `layer` | one of `layers` |
| `z` | only for `objects` |
| `frame` | optional frame (index or name) |
| `animation` | optional animation that loops forever |
| `timeOfDay: true` | the frame is picked from the time of day: `dawn` 05-08 h, `day` 08-18 h, `dusk` 18-21 h, `night` otherwise (sheets `window_sky`, `window_beam`) |
| `clock: {hour, minute}` | on `clock`: draw `hour` sheet frame `hour % 12` and `minute` sheet frame `floor(minute / 5)` at the same `(x, y)` on top of the face |

Props QML changes at runtime (all by id): `desk<i>_monitor` frame (screen
content, section 3.3), `coffee_machine` animation `idle`/`brew` (brew while
someone stands at the `coffee` spot), `printer` `idle`/`print`,
`julius_door` frame `closed`/`ajar`/`open`, `door_lamp` frame
`free`/`busy`/`off` (busy while a question is open), `floor_lamp` `on`/`off`.

### 3.3 desks

Eight entries, `index` 0-7 = the agent's `desk`.

| field | meaning |
|-------|---------|
| `index`, `pod` | desk number; pod `A` (0-3) or `B` (4-7) |
| `facing` | where the seated agent looks: `down` (faces the camera, monitor back to us; desks 0, 1, 4, 5) or `up` (back to the camera, screen visible; desks 2, 3, 6, 7) |
| `desk`, `chair`, `monitor` | prop ids |
| `clutter` | prop ids of the desk's clutter (different on every desk) |
| `seat` | `{x, y, z}`: anchor + z for the seated character |
| `seatNode` | graph node at the seat |
| `approach` | graph node in the corridor in front of the desk; walk there, then straight to `seatNode` |
| `monitorRect` | `[x, y, w, h]` of the monitor sprite (also the hotspot) |
| `screenRect` | `[x, y, w, h]` of the visible screen (desks facing `up`), `null` when only the monitor's back is visible |
| `glow` | `{sprite, x, y, layer}`: where to draw `monitor_glow`; pick its frame from the screen kind (`monitor_glow.kindGlow`) |
| `label` | `{x, y}`: name tag, centred on x, top at y |
| `bubble` | `{x, y}`: emote bubble anchor |

Monitor content: for desks facing `up` set the prop's frame from the agent's
`screen.kind` via `monitor_front.screenKinds` (alternate between the listed
frames every ~0.6 s while the agent is working; `off` when the desk is free).
Desks facing `down` use `monitor_back` frame `on`/`off`.

Pose at the desk, from `facing` (`F` = `up` or `down`):
`sit_type_F` (typing / terminal / running), `sit_read_F` (reading, browsing,
searching), `sit_think_F` (thinking, planning), `sit_idle_F` (waiting, idle),
`sit_raise_F` (has a question / needs permission), `sit_cheer_F` (a turn
finished well, ~2 s), `sit_facepalm_F` (error, ~2 s). `sit_talk_down` is for
the lead (talking into the headset while delegating or messaging).

### 3.4 lead

Same fields as a desk, plus `monitors` (two prop ids, both `monitor_back`),
and `visitorSpots` (two node ids in front of the lead desk where an agent who
is `talking` to the lead stands, pose `talk_up`). The lead faces the camera;
`screenRect` is `null`.

### 3.5 lounge, standing, queue, entrance

`lounge[]`: `{id, x, y, z, facing, node, poses, front?}`. Sit with the anchor
at `(x, y)`, z from `z`. `poses` lists the usable animations (relax, sip,
sleep). `front` (armchairs only) is the prop id of the armrest drawn over the
sitter; it already has the right z.

`standing[]`: `{id, node, x, y, facing, activity, pose, then?}`. Stand at the
node, play `pose`, optionally switch to `then` (`drink`, `stretch`) after a
few seconds.

`queue[]`: ordered, `queue0` is right in front of the Julius door. Agents with
a question for the user fill it from 0, face `up`, play `question_up` (first
in line) or `idle_up`, and show the `question` emote.

`entrance`: `{node, x, y, offscreen: {x, y}}`. Arriving agents start
off-screen below the bottom wall, walk to `node` (play `wave` once there),
then on to their target; leaving agents do the reverse.

`julius`: ids of the door, its status lamp and glow, and the graph node in the
doorway.

### 3.6 graph

`graph.nodes[]`: `{id, x, y}`; `graph.edges[]`: `[idA, idB]`, undirected.
Every seat, spot, queue place, entrance and cat spot is a node, and the graph
is connected (checked by the generator). Walk along shortest paths
(Euclidean edge length), at `characters.walkSpeed` logical px/s, choosing
`walk_down/up/left/right` from the dominant direction of the current segment;
when a walker reaches its spot, switch to the spot's pose. Positions must be
rounded to integers each frame.

### 3.7 cat

`{sprite: "cat", shadow, home, spots: [{node, pose}]}`. The cat idles at a
spot (`sit`, `sleep`, `tail_flick` now and then), then walks along the graph
(`walk_left` / `walk_right`, also for vertical segments) to another random
spot. `home` is the cat bed (sleep there at night).

### 3.8 lights

`{id, sprite, x, y, layer, kind, day?, night?, frame?, animation?,
timeOfDay?, when?, dust?}`.

- `day` / `night`: opacity multiplier for the light by time of day
  (default 1).
- `timeOfDay`: pick the frame like windows do (window beams).
- `when`: draw only while the condition holds (`julius_door!=closed`).
- `dust`: `[x, y, w, h]` area in which to spawn dust motes (`dust` sheet,
  animation `twinkle`, drift 2-4 px/s up and sideways, 3-6 alive, fade out).

Per-desk monitor glows are not in this list; they come from `desks[].glow`
and `lead.glow` and only show while the monitor is on.

Night (from `dusk` on, stronger at `night`): fill the whole scene with
`rgba(16, 18, 48, 0.59)` between the `objects` and `light` layers, then draw
the `light` layer with the `night` multipliers. At day draw lights with the
`day` multipliers and no tint. Draw `vignette` (layer `ambient`) always.

### 3.9 hotspots

`{id, kind, rect: [x, y, w, h], desk?, follows?}`. Kinds: `monitor` (with
`desk`, -1 = lead), `rack` (system panel), `coffee`, `door` (the Julius door;
opens the question panel), `cat` (rect is relative to the cat's anchor, the
cat moves). Hit-test top to bottom in reverse z order; a character standing
in front of a monitor does not block its hotspot.

### 3.10 characters, font

`characters`: `{sprite, shadow, labelOffset, bubbleOffset, walkSpeed}`.
Offsets are relative to a standing character's anchor (seated ones use their
desk's `label` / `bubble`). Draw `shadow_person` at the feet of every standing
or walking character (not when seated).

`font`: `monogram` (CC0, `data/fonts/monogram-extended.ttf`, licence next to
it). Fontconfig family name **`monogram`**. Use `font.pixelSize: 16` = one
font pixel per logical pixel (cap height 7, advance 6, line height 10), then
scale with the scene. Either render text at 16 px inside the unscaled 480x270
scene item and scale that item as a whole (`layer.enabled: true`,
`layer.smooth: false`), or render directly at `16 * scale` px with
`renderType: Text.NativeRendering`. Only multiples of 16 stay crisp.

## 4. Characters

Sheet `characters`: frames 24x32, anchor `[12, 31]`, 8 rows = looks
(0 lead: navy blazer + headset; 1 curly/orange hoodie; 2 blonde bob/sage
sweater; 3 bun + glasses/purple shirt; 4 beanie + beard/flannel; 5 ponytail/
teal tee; 6 grey hair + glasses/yellow cardigan; 7 spiky hair + headphones/
pink tee).

Animations (all in `characters.animations`):

| group | names |
|-------|-------|
| standing | `idle_down/up/left/right` (breathing, blinking) |
| walking | `walk_down/up/left/right` (4 frames, head bob, arm swing) |
| gestures | `wave` (arrival), `cheer` (success), `facepalm` (error, no loop), `question` (hand up, facing camera), `question_up` (hand up, facing the door), `talk_down/up/left/right` (gesturing), `drink` (mug), `stretch` (no loop), `carry_down` (box) |
| seated, back to camera | `sit_type_up`, `sit_idle_up`, `sit_read_up`, `sit_think_up`, `sit_cheer_up`, `sit_facepalm_up`, `sit_raise_up` |
| seated, facing camera | `sit_type_down`, `sit_idle_down`, `sit_read_down`, `sit_think_down`, `sit_cheer_down`, `sit_facepalm_down`, `sit_raise_down`, `sit_talk_down` |
| lounge | `lounge_relax_*`, `lounge_sip_*`, `lounge_sleep_*` for `down`, `left`, `right` |

Sheet `cat`: 18x13, anchor `[9, 12]`: `walk_left`, `walk_right`,
`stand_left`, `stand_right` (tail flick), `sit`, `tail_flick`, `sleep`.

Emotes (sheet `emote`, 13x15, anchor = tip of the bubble's tail): `question`,
`exclaim`, `dots` (3-frame typing dots), `z` (sleeping, 3 frames), `heart`,
`sweat`, `sparkle`, `music`, `check`, `cross`, `coffee`, `bulb`. Bob them 1 px
up and down at ~1 Hz. Suggested mapping: asking/permission `question`, error
`sweat` or `exclaim`, success `sparkle`/`check`, lounge sleep `z`, thinking
`dots`, coffee break `coffee`, lounge with the record player `music`, the cat
when petted `heart`.

## 5. Overlays and how to animate them

| sprite | use |
|--------|-----|
| `shadow_person`, `shadow_cat`, `sh_*` | drop shadows (already placed for props) |
| `window_sky`, `window_beam` | time of day frames; fade between frames over a minute when the period changes |
| `monitor_glow` | frames `cool green warm red violet`; flicker opacity 0.85-1.0 at ~3 Hz while typing |
| `pool_warm`, `pool_small`, `halo_warm`, `strip_glow` | lamp light; static, stronger at night |
| `rack_glow` | animation `pulse`; brighten while the system is busy |
| `door_glow` | frames `busy`/`free`, follows `door_lamp` |
| `door_spill` | only while the Julius door is not closed |
| `steam` | `rise`; show above `coffee_machine` only while brewing and for 3 s after |
| `dust` | spawn inside `lights[].dust` at day only |
| `vignette` | full scene, static |

Props with built-in loops: `server_rack` / `net_cabinet` `blink`,
`water_cooler` `bubble`, `record_player` `spin`, plants `sway`
(`plant_monstera`, `plant_ficus`, `plant_hanging`; desynchronise them).

## 6. Monitors and the UI kit

- `monitor_front` (20x18, anchor `[10, 17]`): frames `off code_a code_b
  terminal_a terminal_b doc web search plan chat error question diff brief`;
  `screen` = screen rect inside the frame; `screenKinds` maps `screen.kind`
  (from the agent) to frames.
- `monitor_back` (20x18): `on`, `off`.
- `monitor_big` (208x140): the zoomed monitor. `screen` = `[14, 12, 180,
  100]`, the area for live text (dark already). Draw the text into that rect
  (monogram 16 px: 30 columns x 10 lines at 1x), then `monitor_big_glass`
  (180x100, sheen + scanlines) on top at the same rect position.
- Panels: `ui_panel_dark`, `ui_panel_light` (9-slice 8). `ui_tag` (name tag,
  9-slice 3, height 11), `ui_row_highlight` (9-slice 2).
- Controls: `ui_button`, `ui_button_accent` (frames `normal hover pressed`,
  9-slice `[5, 4, 5, 5]`, height 14; text baseline 3 px from the top);
  `ui_tab` (`active inactive`, height 12); `ui_textfield` (`normal focus`,
  height 14) + `ui_caret` (`blink`); `ui_check` (`off on radio_off radio_on`,
  9x9); `ui_scroll_track` + `ui_scroll_thumb` (`normal hover`, 6 wide,
  vertical 9-slice).
- Bubbles: `bubble` (9-slice 5) + `bubble_tail` (anchor at its top centre,
  attach to the bubble's bottom edge, 1 px overlap).
- Icons: `icons` (9x9): `cpu ram gpu disk net temp fan question check cross
  close terminal file globe search plan chat clock cat code power user door
  bolt`. Text colour `#f2e8dc`, accent `#f0a24a`.
- Gauges: `ui_bar_bg` (9-slice 3, height 7) with `ui_bar_fill` inside, inset
  1 px (frames `ok warn hot info accent`, height 5, stretch horizontally);
  `ui_led` (5x2) and `ui_led_dot` (2x2), frames `off ok warn hot info`, for
  segmented meters (stack with 1 px gaps).
- `fan` (20x20, `spin`): scale the fps with GPU fan speed / load.
- `sysroom` (232x150): PC case cut-away for the system panel. `regions`
  (frame coordinates): `cpuCores` rect for one LED column per core (5 px
  wide columns of `ui_led`, filled bottom-up: ok < 55 %, warn < 80 %, hot
  above), `ramSticks` (4 rects, fill bottom-up with `ui_led` cropped to 4 px:
  used memory spread over the sticks), `disk` + `diskLed` (blink on I/O),
  `gpuFans` (two `fan` positions), `gpuBar`, `gpuMemBar`, `psuFan`, net LEDs
  (`netRxLed`, `netTxLed`, `net2RxLed`, `net2TxLed`: `ui_led_dot`, blink with
  traffic), `thermoTube` (fill bottom-up in `#d0503f` with the CPU
  temperature, 30-95 °C), `thermoBulb`, and `*Label` points for monogram
  labels.

Palette reference for QML-drawn things (text, fills): text `#f2e8dc`, dim
text `#9c8ea6`, accent `#f0a24a`, panel `#251c2c`, ok `#7fd8ae`, warn
`#f6dc6a`, hot `#d0503f`, info `#a8d0ec`, outline `#1e1724`.
