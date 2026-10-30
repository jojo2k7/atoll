# Controls

Every way of telling the island to do something lives in one place: the
`controls` section of the config, edited on **Settings → Controls**. Any
trigger can carry any action.

## Triggers

| Key | Trigger | Default |
| --- | --- | --- |
| `controls.click` | Click | `expand` |
| `controls.doubleClick` | Double click | `none` |
| `controls.middleClick` | Middle click | `officeOrPlayPause` |
| `controls.rightClick` | Right click | `settings` |
| `controls.longPress` | Press and hold | `assistant` |
| `controls.scrollUp` | Scroll up | `volumeUp` |
| `controls.scrollDown` | Scroll down | `volumeDown` |
| `controls.hover` | Rest the pointer on the island | `peek` |

Tuning that belongs to the triggers sits next to them:

| Key | Meaning | Default |
| --- | --- | --- |
| `controls.longPressMs` | How long a press has to be held | `450` |
| `controls.hoverDelay` | How long the pointer has to rest, in ms | `180` |
| `controls.collapseOnLeave` | Fold the dashboard when the pointer leaves | `true` |
| `controls.volumeStep` | Percent per scroll step | `5` |

While the island shows something with its own buttons - the assistant, the
office, a share offer, a focus session in detail - those buttons win and the
pointer triggers stay out of the way, exactly as before.

Actions other than the volume steps fire at most once per scroll gesture, so a
flick of the wheel does not toggle the office ten times.

If the middle click carries `office` or `officeOrPlayPause`, holding it is a
peek: the office opens on press and closes again on release.

## Actions

| Id | What it does |
| --- | --- |
| `none` | Nothing |
| `expand` | Open or fold the dashboard |
| `collapse` | Fold the island back to its idle size |
| `peek` | Show what is playing while the pointer rests (hover only) |
| `dismiss` | Dismiss whatever activity the island is showing |
| `settings` | Open this window |
| `office` | Open or close the Claude office |
| `officeOrPlayPause` | The office while an agent is working, otherwise play / pause |
| `assistant` | Start a question to the assistant |
| `playPause`, `next`, `previous` | Control the active player |
| `volumeUp`, `volumeDown` | Change the output volume by `controls.volumeStep` |
| `focus` | Start a focus session, or stop the running one |
| `dnd` | Toggle do not disturb |

The list is defined once, in `qml/Actions.qml`, and both the island and the
settings window read it.

## Keyboard shortcuts

`controls.shortcuts` maps an action id to a key combination in Qt's portable
text form, for example:

```json
"controls": {
  "shortcuts": {
    "assistant": "Meta+Space",
    "office": "Meta+Shift+O"
  }
}
```

The island registers them with KDE's global shortcut service (KGlobalAccel), so
they work in every app and also show up under **System Settings → Shortcuts →
Atoll**, where they can be changed too. An empty string, or a missing entry,
leaves the action unbound. Without KGlobalAccel at build time the section is
hidden and `atollctl action <id>` is the way to bind keys.

## From scripts

```sh
atollctl action office     # any id from the table above
```

which is the D-Bus method `org.atoll.Atoll.action(id)`.

## Older configs

Configs written before the Controls page keep working. On load, these keys are
read into their new place when the new one is not set:

| Old key | New key |
| --- | --- |
| `behavior.clickAction` | `controls.click` |
| `behavior.middleClickAction` | `controls.middleClick` |
| `behavior.rightClickAction` | `controls.rightClick` |
| `behavior.scrollAdjustsVolume: false` | `controls.scrollUp` / `scrollDown` = `none` |
| `behavior.volumeStep` | `controls.volumeStep` |
| `behavior.hoverPeek` / `behavior.expandOnHover` | `controls.hover` (`peek`, `expand` or `none`) |
| `behavior.hoverDelay` | `controls.hoverDelay` |
| `behavior.collapseOnLeave` | `controls.collapseOnLeave` |
| `ai.longPressMs` | `controls.longPressMs` |
