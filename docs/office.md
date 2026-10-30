# The Claude Code office

A pixel-art office that mirrors running Claude Code sessions. Every agent in a
session - the lead and each subagent - has a desk; what it is doing right now
decides where it is and what its monitor shows.

It costs the model nothing. Claude Code reports every step to Atoll through
hooks, and a hook that prints nothing adds nothing to the context.

```
claude ──hook JSON on stdin──> atoll --claude-hook ──one line──> $XDG_RUNTIME_DIR/atoll/claude.sock
                               (async, silent, exit 0)          OfficeService (App.office) ──> OfficeWindow.qml
```

## Transport

- Socket: `QStandardPaths::RuntimeLocation + "/atoll/claude.sock"`, a
  `QLocalServer` with `UserAccessOption`. The directory is created `0700`.
- Not D-Bus on purpose: sessions running as root cannot reach the user's
  session bus, but root can open the user's socket.
- The hook looks for the socket at `$XDG_RUNTIME_DIR/atoll/claude.sock` first,
  then at every `/run/user/*/atoll/claude.sock`, and writes the event to each
  one it finds.
- One compact JSON object per line, connection closed after the write. Lines
  over 64 KiB are dropped by the server.

## `atoll --claude-hook`

Decided before any application object exists (like `--permission-hook`), runs
on a `QCoreApplication`, never touches the display. It must:

- read the whole hook payload from stdin,
- build the event below with `OfficeEvent::compact(const QJsonObject &)`,
- write it, waiting at most 300 ms to connect and 300 ms to flush,
- print **nothing** on stdout or stderr and **always exit 0**.

### Event (`"v": 1`)

| key   | meaning |
|-------|---------|
| `v`   | `1` |
| `e`   | `hook_event_name` |
| `s`   | `session_id` |
| `cwd` | `cwd` |
| `a`   | `agent_id` if present, else `""` |
| `at`  | `agent_type` if present, else `""` |
| `t`   | `tool_name`, else `""` |
| `arg` | short argument, single line, at most 120 chars (rules below) |
| `ok`  | only on `PostToolUse`/`PostToolUseFailure`: `false` for `PostToolUseFailure`, or when `tool_response` is an object with `is_error == true` or a non-empty `error`; `true` otherwise |
| `msg` | at most 200 chars: Notification `message`; SendMessage text; first AskUserQuestion question; UserPromptSubmit is **not** forwarded (privacy) |
| `to`  | SendMessage recipient |
| `nt`  | Notification `notification_type` |
| `user`| name of the user the hook runs as |
| `ppid`| parent pid of the hook process (the claude process) |
| `ts`  | ms since epoch |

`arg` rules (from `tool_input`):

- `Bash`: `description` if non-empty, else `command`
- `Read`, `Edit`, `Write`, `MultiEdit`, `NotebookEdit`: basename of `file_path` / `notebook_path`
- `Grep`, `Glob`: `pattern`
- `WebFetch`: host of `url`; `WebSearch`: `query`
- `Agent`, `Task`: `description`
- `SendMessage`: `to` (fallbacks `recipient`, `name`); text in `msg` from `message` (fallbacks `content`, `text`, `summary`)
- `AskUserQuestion`: `questions[0].question` (also into `msg`)
- `Skill`: `skill`; `TaskCreate`/`TaskUpdate`: `subject`, else `taskId`
- anything else: empty

Newlines collapse to spaces; anything longer is cut with `…`.

## `OfficeService` (`App.office`)

Lives in `src/office/`. Owns the socket server and the model. Enabled by
config `office.enabled` (default `true`); disabled means no socket.

### Properties

| name | type | meaning |
|------|------|---------|
| `active` | bool | at least one live session |
| `open` | bool, writable | the office window is showing |
| `sessions` | list | `{id, project, cwd, user, startedAt, lastAt, toolCalls, waiting}`, newest `lastAt` first; `project` is the basename of `cwd` |
| `currentSession` | string, writable | the session on screen; follows the most recently active one until the user picks one with `selectSession` |
| `agents` | list | agents of `currentSession` (below) |
| `feed` | list | last 20 `{from, to, text, at}` SendMessage lines of `currentSession` |
| `working`, `resting`, `waiting` | int | counts in `currentSession` (`resting` = state `idle`) |
| `toolCalls` | int | tool calls in `currentSession` |
| `sessionStart` | qint64 | ms since epoch, `currentSession` |

Agent object:

```
{ key, name, type, lead, state, activity, label, detail, desk, look,
  since, toolCalls, waitingFor, recent: [{t, arg, ok, at}] }
```

- `key`: `agent_id`, or `"lead"`.
- `name`: `"Lead"` for the lead; for subagents the `agent_type` turned into a
  word (`general-purpose` → `General`, `Explore` → `Explore`) plus a counter
  per type and session: `Explore 1`, `Explore 2`.
- `state`: `working` | `idle` | `waiting` | `leaving`.
- `activity`: `typing` | `terminal` | `reading` | `browsing` | `delegating` |
  `talking` | `asking` | `planning` | `archiving` | `thinking` | `error` | `idle`.
- `label`: the short tag shown above the head, from the table below.
- `detail`: the last `arg`.
- `desk`: lead is `-1`; subagents get the lowest free index `0..7`; a ninth
  concurrent subagent gets `-2` (standing).
- `look`: lead `0`; subagents `1 + qHash(agent_id) % 7`.
- `waitingFor`: `""` or `"user"`.
- `recent`: last 5 tool calls, newest first.

### Tool → activity

| tools | activity |
|-------|----------|
| `Edit Write MultiEdit NotebookEdit` | typing |
| `Bash` (arg starting `graphify` → reading), `BashOutput KillShell Monitor`, other `mcp__*` | terminal |
| `Read Grep Glob LSP ToolSearch Skill` | reading |
| `WebFetch WebSearch`, `mcp__*` containing `browser` or `chrome` | browsing |
| `Agent Task` | delegating |
| `SendMessage` | talking |
| `AskUserQuestion` | asking |
| `TaskCreate TaskUpdate TaskList TaskGet TodoWrite EnterPlanMode ExitPlanMode` | planning |
| anything else | thinking |

Labels: typing "writing code", terminal "running a command", reading "reading
files", browsing "on the web", delegating "briefing a teammate", talking
"messaging <to>", asking "has a question", planning "updating the plan",
archiving "compacting memory", thinking "thinking", error "hit an error", idle
"on a break"; a permission wait is "needs permission".

### Events → state

Unknown sessions and unknown `agent_id`s are created on first sight - sessions
often start before Atoll does. `a` empty means the lead.

- `SessionStart`: session with an idle lead.
- `UserPromptSubmit`: lead working, thinking.
- `PreToolUse`: agent working, activity from the table, `detail = arg`,
  `toolCalls++` on agent and session, push to `recent`. `AskUserQuestion` makes
  it `waiting` / `waitingFor: "user"` and emits `attention`. `SendMessage`
  appends to `feed`.
- `PostToolUse`, `PostToolUseFailure`: clears a wait; `ok == false` sets
  activity `error`, otherwise `thinking`; updates `ok` in `recent`.
- `PermissionRequest`: agent `waiting` for the user, "needs permission",
  `attention`.
- `Notification`: `permission_prompt` as `PermissionRequest` on the lead;
  `idle_prompt` puts the lead to `idle`; anything else only emits `attention`.
- `PreCompact`: lead activity `archiving`.
- `Stop`: lead `idle`, wait cleared.
- `SubagentStart`: subagent working, thinking.
- `SubagentStop`: subagent `leaving`, removed 6 s later.
- `SessionEnd`: everyone `leaving`, session removed 6 s later.
- No event for 45 min: session removed. A subagent quiet for 20 min leaves.

Property change signals are coalesced (at most every 100 ms).

### Invokables and signals

`toggle()`, `show()`, `hide()`, `selectSession(id)`, `ingest(QJsonObject)`
(what the socket calls; public for tests), signal
`attention(QString session, QString agent, QString text)`.

## Opening it

- Middle button on the island by default. `controls.middleClick`:
  `officeOrPlayPause` (default: office while a session is live, play/pause
  otherwise) or `office`. A short click toggles; holding the button for more
  than 350 ms opens it only until release (peek). Any other trigger or a
  keyboard shortcut can carry `office` too - see [Controls](controls.md).
- `atoll --office`, D-Bus `org.atoll.Atoll.office()`, `atollctl office` toggle.
- Esc, a middle click or a click on the backdrop closes it.

The window is its own layer surface (`ShellWindow::configureOffice`, scope
`atoll-office`, overlay layer, all four anchors, exclusive keyboard) created by
a `Loader` only while `App.office.open`, on the first output in
`App.shell.targets`. Closed, it costs nothing.

## Sprites

`scripts/gen-office-sprites.py` (Pillow) draws everything into
`data/office/`; the PNGs are checked in and listed as module resources under
`qrc:/qt/qml/Atoll/data/office/`. Always drawn with `smooth: false`.

Characters: `characters.png`, frames 16×24, one row per look (8 rows, look 0 is
the lead), columns:

| col | frame |
|-----|-------|
| 0 | standing, facing down |
| 1, 2 | walking down |
| 3, 4 | walking up |
| 5, 6 | walking sideways (facing right; mirror for left) |
| 7, 8 | seated at a desk seen from behind, typing |
| 9 | seated in the lounge, facing down |
| 10 | hand raised, facing down |

Furniture, one PNG each, sizes in pixels (frames side by side):

| file | size | frames |
|------|------|--------|
| `floor.png` | 16×16 | 1 |
| `wall.png` | 16×40 | 1 |
| `window.png` | 40×24 | 1 |
| `bookshelf.png` | 32×32 | 1 |
| `desk.png` | 40×20 | 1 |
| `monitor.png` | 16×14 | 8: off, code A, code B, terminal A, terminal B, document, globe, error |
| `chair.png` | 14×14 | 1 (back view) |
| `lead_desk.png` | 56×24 | 1 |
| `plant.png` | 14×22 | 1 |
| `plant_small.png` | 10×14 | 1 |
| `sofa.png` | 48×20 | 1 |
| `armchair.png` | 18×18 | 1 |
| `coffee_table.png` | 24×14 | 1 |
| `coffee_machine.png` | 18×28 | 2 (idle, brewing) |
| `server_rack.png` | 24×40 | 2 (blink A, blink B) |
| `filing_cabinet.png` | 16×24 | 1 |
| `floor_lamp.png` | 8×32 | 1 |
| `whiteboard.png` | 48×28 | 1 |
| `door.png` | 24×36 | 2 (closed, open) |
| `rug.png` | 96×64 | 1 |
| `bubble.png` | 10×10 | 4: `?`, `!`, `...`, `z` |
