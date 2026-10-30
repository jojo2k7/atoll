// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick

/**
 * Everything the island can be told to do, and everything that can tell it.
 *
 * Both processes read this list: the island to run an action, the settings
 * window to offer it. Any trigger can carry any action - a scroll that opens
 * the office is odd, but it is the user's island. See docs/controls.md.
 */
QtObject {
    /**
     * The actions. `group` sorts them in the picker; `shortcut` is false for
     * the few that only make sense with a pointer on the island.
     */
    readonly property var all: [
        { id: "none", label: qsTr("Nothing"), group: "", icon: "none", shortcut: false },

        { id: "expand", label: qsTr("Open dashboard"), group: qsTr("Island"), icon: "expand", shortcut: true },
        { id: "collapse", label: qsTr("Collapse"), group: qsTr("Island"), icon: "collapse", shortcut: true },
        { id: "peek", label: qsTr("Peek at what is playing"), group: qsTr("Island"), icon: "peek", shortcut: false },
        { id: "dismiss", label: qsTr("Dismiss current activity"), group: qsTr("Island"), icon: "dismiss", shortcut: true },
        { id: "settings", label: qsTr("Open settings"), group: qsTr("Island"), icon: "settings", shortcut: true },

        { id: "office", label: qsTr("Claude office"), group: qsTr("Claude"), icon: "office", shortcut: true },
        { id: "officeOrPlayPause", label: qsTr("Claude office, else play / pause"), group: qsTr("Claude"), icon: "office", shortcut: true },
        { id: "assistant", label: qsTr("Ask the assistant"), group: qsTr("Claude"), icon: "assistant", shortcut: true },

        { id: "playPause", label: qsTr("Play / pause"), group: qsTr("Media"), icon: "playPause", shortcut: true },
        { id: "next", label: qsTr("Next track"), group: qsTr("Media"), icon: "next", shortcut: true },
        { id: "previous", label: qsTr("Previous track"), group: qsTr("Media"), icon: "previous", shortcut: true },
        { id: "volumeUp", label: qsTr("Volume up"), group: qsTr("Media"), icon: "volumeUp", shortcut: true },
        { id: "volumeDown", label: qsTr("Volume down"), group: qsTr("Media"), icon: "volumeDown", shortcut: true },

        { id: "focus", label: qsTr("Start or stop focus"), group: qsTr("System"), icon: "focus", shortcut: true },
        { id: "dnd", label: qsTr("Do not disturb"), group: qsTr("System"), icon: "dnd", shortcut: true }
    ]

    /**
     * The pointer triggers, in the order the settings page lists them.
     * `fallback` is the default written in config.cpp.
     */
    readonly property var triggers: [
        { key: "click", label: qsTr("Click"), fallback: "expand" },
        { key: "doubleClick", label: qsTr("Double click"), fallback: "none" },
        { key: "middleClick", label: qsTr("Middle click"), fallback: "officeOrPlayPause" },
        { key: "rightClick", label: qsTr("Right click"), fallback: "settings" },
        { key: "longPress", label: qsTr("Press and hold"), fallback: "assistant" },
        { key: "scrollUp", label: qsTr("Scroll up"), fallback: "volumeUp" },
        { key: "scrollDown", label: qsTr("Scroll down"), fallback: "volumeDown" },
        { key: "hover", label: qsTr("Rest the pointer"), fallback: "peek" }
    ]

    /** Actions a key combination can be bound to. */
    readonly property var shortcutActions: all.filter(a => a.shortcut)

    function find(id) {
        for (const a of all) {
            if (a.id === id) {
                return a
            }
        }
        return all[0]
    }

    function label(id) {
        return find(id).label
    }
}
