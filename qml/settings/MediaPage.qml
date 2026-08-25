// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Media and lyrics")
    description: qsTr("Anything that speaks MPRIS2 - Spotify, a browser tab, a local player - reaches the island the same way.")

    SettingsGroup {
        title: qsTr("Now playing")

        BoolSetting {
            label: qsTr("Announce new tracks")
            description: qsTr("The island unfolds for a moment whenever a track starts.")
            key: "media.showOnPlay"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Announcement length")
            key: "media.peekDuration"
            defaultValue: 4200
            from: 500
            to: 20000
            step: 100
            suffix: " ms"
            enabled: Cfg.get("media.showOnPlay", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Cover art")
            key: "media.showArt"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Album name")
            description: qsTr("Shown next to the artist when there is no lyric line to show instead.")
            key: "media.showAlbum"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Cover in the resting island")
            description: qsTr("A thumbnail next to the clock while something plays.")
            key: "media.idleBadge"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Players")

        ListSetting {
            label: qsTr("Preferred players")
            description: qsTr("Names or bus names, best first. A preferred player wins ties, but never outranks one that is actually playing. Example: spotify, elisa")
            key: "media.preferred"
            placeholder: qsTr("spotify, elisa")
        }

        ListSetting {
            label: qsTr("Ignored players")
            description: qsTr("Players the island should never speak for.")
            key: "media.blocked"
            placeholder: qsTr("firefox, chromium")
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Spectrum")

        NumberSetting {
            label: qsTr("Bars")
            key: "media.visualizerBars"
            defaultValue: 26
            from: 4
            to: 64
        }

        TextSetting {
            label: qsTr("cava")
            description: qsTr("\"auto\" uses cava when it is installed, \"off\" never does, or give a path.")
            key: "media.cava"
            defaultValue: "auto"
            placeholder: "auto"
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Lyrics")

        BoolSetting {
            label: qsTr("Look up lyrics")
            description: qsTr("Reads an .lrc file next to local tracks. For everything else it asks lrclib.net, sending only artist, title, album and duration. This is the only request Atoll ever makes.")
            key: "lyrics.enabled"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Line in the island")
            description: qsTr("Replaces the artist line with the words being sung.")
            key: "lyrics.showInIsland"
            defaultValue: true
            enabled: Cfg.get("lyrics.enabled", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Panel in the dashboard")
            key: "lyrics.showInExpanded"
            defaultValue: true
            enabled: Cfg.get("lyrics.enabled", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Click a line to seek")
            key: "lyrics.seekOnClick"
            defaultValue: true
            enabled: Cfg.get("lyrics.enabled", true)
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Timing offset")
            description: qsTr("Positive values show each line earlier.")
            key: "lyrics.offsetMs"
            defaultValue: 0
            from: -5000
            to: 5000
            step: 100
            suffix: " ms"
            enabled: Cfg.get("lyrics.enabled", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Keep a local copy")
            description: qsTr("Caches fetched lyrics so the same track is only ever looked up once.")
            key: "lyrics.cache"
            defaultValue: true
            enabled: Cfg.get("lyrics.enabled", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }
}
