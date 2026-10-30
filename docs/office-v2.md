# The Claude Code office, v2

Supersedes the parts of `office.md` it names; everything else there (transport,
`--claude-hook`, event table, state machine) stays valid.

Goals of v2: the office lives **inside the island**, it is drawn at game-studio
quality, every monitor can be looked into, questions can be answered right
there, and long shell tasks show what the machine is going through.

## 1. Inside the island

- New island mode `"office"` while `App.office.open` (and not locked). Priority:
  below `"ai"`, above everything else.
- In that mode the island is as wide and tall as the office view asks for; the
  usual `Cfg.maxWidth` cap does not apply. It is capped at 94 % of the screen
  width; `Stage.neededHeight` already grows the surface.
- It does not collapse on pointer leave. It closes on Esc, middle click, the
  close button, or `App.office.hide()`.
- While open the island takes keyboard focus (`App.shell.setKeyboardFocus`),
  and gives it back when closed.
- The separate window from v1 goes away: `qml/OfficeWindow.qml`, the
  `Instantiator` in `Main.qml`, and `ShellWindow::configureOffice` are removed.
- Opening/closing is a morph: the island grows from the pill into the office.
- The scene renders at an integer scale (`floor(min(0.94*screenW/W, 0.86*screenH/H))`,
  at least 2) where W×H is the logical scene size from the layout file.

## 2. Monitor preview

Each agent carries a `screen` describing what is on its monitor right now. The
hook adds it to the event as `pv` (built from `tool_input`, and on
`PostToolUse`/`PostToolUseFailure` from `tool_response`/`error`):

```
pv: { kind, title, text }      text <= 4000 chars, lines joined by "\n"
```

| tool | kind | title | text |
|------|------|-------|------|
| Edit / MultiEdit | `diff` | file basename | `-` old lines, `+` new lines (first 60 lines) |
| Write | `code` | file basename | first 60 lines of `content` |
| NotebookEdit | `code` | notebook basename | `new_source` |
| Bash (pre) | `terminal` | `description` or command | `$ <command>` |
| Bash (post) | `terminal` | same | `$ <command>` + last 50 lines of `stdout`, then `stderr`; failure: `error` |
| Read | `doc` | file basename | on post: first 60 lines of whatever text the response holds (`file.content`, `content`, or the JSON stringified), else the path |
| Grep / Glob | `search` | pattern | on post: first 40 lines of the response text |
| WebFetch / WebSearch | `web` | host / query | `prompt` on pre; first 40 lines of the response on post |
| Agent / Task | `brief` | description | first 40 lines of `prompt` |
| SendMessage | `chat` | `to` | message |
| TaskCreate / TaskUpdate / TodoWrite | `plan` | subject | subjects / todos one per line |
| AskUserQuestion | `question` | header | question text |
| anything else | `tool` | tool name | tool_input as compact JSON, cut |

Response text extraction is defensive: response shapes for Read/Grep/WebFetch
are not documented, so walk strings/objects generically and never fail.

Agent object gains:

```
screen: { kind, title, text, at }     // last pv seen for this agent
running: bool                          // a Bash PreToolUse without its Post yet
runningSince: ms                        // when that started
busy: bool                             // running for more than 3 s
```

`UserPromptSubmit` text is still never forwarded.

## 3. Answering questions

A second hook mode, **synchronous**, only for AskUserQuestion:

```json
{ "matcher": "AskUserQuestion",
  "hooks": [{ "type": "command", "command": "<atoll> --claude-ask", "timeout": 600 }] }
```

`atoll --claude-ask`:

1. Reads the PreToolUse payload. If no socket is reachable: exit 0, print
   nothing (the CLI then asks in the terminal as usual).
2. Generates an id, sends
   `{"v":1,"e":"Ask","id":…,"s":…,"a":…,"at":…,"cwd":…,"user":…,"ts":…,"questions":[…]}`
   to the first candidate socket, and waits up to 1.5 s for `{"id":…,"ack":true}`
   echoed back - proof that end understands "Ask" rather than leaving the CLI
   blocked against one that doesn't. No ack → try the next candidate socket
   the same way. Once acked, that connection is kept open, waiting for one
   reply line (max 590 s more).
3. Reply `{"id":…,"answers":{"<question text>":"<answer>"}}` → prints
   ```json
   {"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow",
     "updatedInput":{"questions":[…original…],"answers":{…}}}}
   ```
   Multi-select answers are the chosen labels joined by `", "`; a custom answer
   is the typed text.
4. Reply `{"id":…,"cancel":true}`, a closed connection, or the timeout → exit
   0 with no output (the terminal asks instead).

`OfficeService`:

- `questions` property: open asks, oldest first:
  `{id, session, agentKey, agentName, project, questions:[{question, header, options:[{label, description}], multiSelect}], at}`.
- An ask is acked - `{"id":…,"ack":true}` on the same connection - the moment
  it is registered, before anything else.
- `answerQuestion(id, answersMap)`, `cancelQuestion(id)` invokables write the
  reply line and drop the entry. A dropped connection drops the entry too.
- A new ask emits `attention(…)` and, when `office.askInIsland` (default
  `true`), opens the office with that question in front.
- The agent who asked is `waiting` / `waitingFor: "user"` until answered.

The island UI mirrors the CLI picker 1:1: a header row with one tab per
question (plus "Submit" when there are several), the question text, numbered
options with label and dimmed description, `↑`/`↓` to move, `Enter` or the
number key to pick, `Space` to toggle in multi-select, a final numbered row
"Type something." that turns into a text field, `Tab`/`←`/`→` between
questions, `Esc` = "answer in terminal" (cancel). Pointer works for all of it.

## 4. System load

`SystemMonitor` (`App.office.system`), sampling only while something shows it
(`active` property set by QML) at 1 Hz:

| property | source |
|----------|--------|
| `cpu` (0-100), `cores` (list 0-100), `load1` | `/proc/stat`, `/proc/loadavg` |
| `memUsed`, `memTotal`, `swapUsed`, `swapTotal` (bytes) | `/proc/meminfo` |
| `cpuTemp` | hwmon `k10temp` (Tctl) or `coretemp` |
| `gpu` `{name, util, memUsed, memTotal, temp, power, fan}` | `nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw,fan.speed --format=csv,noheader,nounits` (async QProcess), else amdgpu sysfs, else empty |
| `diskRead`, `diskWrite` (B/s), `disks` `[{name, read, write}]` | `/proc/diskstats`, whole disks only |
| `diskTemp` | hwmon `nvme` |
| `netRx`, `netTx` (B/s) | `/proc/net/dev` without `lo` |
| `processes` `[{pid, name, cpu, rss}]` | top 6 by CPU among descendants of `focusPid` |

`focusPid` (writable) is the claude process of the agent being looked at
(`ppid` from its events, kept per session as `pid`). The office shows the
monitor automatically as a small overlay while any agent is `busy`, and in full
when the server rack or a busy agent's monitor is opened.

## 5. Art direction

Game-studio pixel art, cosy and alive. All art is generated by
`scripts/gen-office-sprites.py` into `data/office/`; the scene is described by
data so the art can evolve without QML changes:

- `data/office/manifest.json` - every sprite sheet: file, frame size, named
  animations (frame list, fps, loop), anchor point (feet for characters).
- `data/office/layout.json` - logical scene size, draw layers, every prop with
  position / sprite / animation / z-sort point, the 8 desks (desk, chair, seat
  point and facing, monitor position, the monitor's screen rectangle for the
  zoom), the lead desk, lounge seats with facing, standing spots, the queue in
  front of the user's door, entrance, a walk graph (nodes + edges) for path
  finding, light sources (for glow / light pools), hotspots (monitors, server
  rack, coffee machine, door) with their click rectangles.
- A bundled pixel font (CC0 or OFL, licence file next to it) under
  `data/fonts/`, used for every text in the office.
- A pixel UI kit: 9-slice panels, buttons (normal/hover/pressed), text field,
  checkbox, tab, scrollbar, speech bubbles, icons (cpu, ram, gpu, disk, net,
  temp, question, check, cross, close, terminal), gauge and bar pieces, a large
  monitor bezel for the zoomed preview, a rack/case view for the system panel.
- Ambient life: coffee steam, swaying plants, blinking rack LEDs, a wall clock,
  day/night sky in the windows following the real time, light pools under
  lamps and from monitors, dust motes in window light, an office cat that
  wanders and naps, per-desk clutter (mugs, papers, sticky notes, keyboards,
  figurines) so no two desks look the same.
- Characters with personality: breathing and blinking idle, typing, reading,
  drinking coffee, stretching, a small cheer when a turn finishes well, a
  facepalm on errors, sleeping in the lounge, waving when they arrive.
