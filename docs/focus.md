# Focus sessions

Open the island, click the small **Focus** tile beside the clock, then choose **Start focus**. The focus card offers pause/resume,
extension by a configurable number of minutes, a break, and stop. In its compact
state the notch shows a breathing light and an optional countdown. The dashboard
offers **Park a thought**, which reveals a thought parking field: Enter saves a thought locally without leaving
Atoll. The three latest thoughts are shown; copy all before clearing the list.
Up to 100 thoughts of 500 characters are retained.

**Settings → Focus** controls intention, work/break durations, long-break cadence,
automatic transitions, extension duration, audio file, volume, fade time, sound
during breaks, lock behavior, media pausing, notification policy, accent, breathing
cycle and countdown visibility. Times apply to the next phase; sound and appearance
update live. Audio is played locally through Qt Multimedia and loops until paused.
The audio file is personal data and is not included in the repository.

Focus pauses on screen lock by default and always pauses after detecting a system
suspend. Unlock never resumes it automatically. A restart restores an interrupted
session paused. Completed phases and thoughts remain local in `focus-state.json`
in Qt's application data directory. An explicit stop retains thoughts. Skipping
an unfinished focus phase does not increase the completion count.

Quiet mode suppresses Atoll's notification popups during active work, retaining
the existing notification history. Critical notifications may pass through. This
setting does not change the desktop notification daemon's own Do Not Disturb
state. Media paused at session start is not automatically resumed.

For a global keyboard shortcut, bind any of these in the desktop's shortcut settings:

```sh
atollctl focus start
atollctl focus pause   # toggle pause/resume
atollctl focus extend
atollctl focus next
atollctl focus stop
atollctl focus status  # JSON state inside the D-Bus reply
```

Build requires `qt6-multimedia` in addition to the existing dependencies. Run
`ctest --test-dir build-focus --output-on-failure` for the session lifecycle test.

The focus detail view replaces the dashboard inside the notch. Its back button
returns to the dashboard; leaving the notch collapses it (unless editing a thought).
The media hover uses a three-row layout based on the supplied reference: artwork
and metadata, seek bar, then playback controls. Width and controls are configurable
in Settings → Media → Hover player. The device-shaped button opens the player.
