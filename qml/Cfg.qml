// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * Typed view onto ~/.config/atoll/atoll.json.
 *
 * Everything hangs off one notifying property, so editing the file - or moving
 * a switch in the settings window, which writes the same file - re-evaluates
 * every binding in the island without a restart.
 */
QtObject {
    id: cfg

    readonly property var data: App.config.data

    readonly property var island: data.island ?? ({})
    readonly property var appearance: data.appearance ?? ({})
    readonly property var effects: data.effects ?? ({})
    readonly property var modules: data.modules ?? ({})
    readonly property var osd: data.osd ?? ({})
    readonly property var notifications: data.notifications ?? ({})
    readonly property var media: data.media ?? ({})
    readonly property var lyrics: data.lyrics ?? ({})
    readonly property var lockScreen: data.lockScreen ?? ({})
    readonly property var sharing: data.sharing ?? ({})
    readonly property var behavior: data.behavior ?? ({})
    readonly property var ai: data.ai ?? ({})
    readonly property var clock: data.clock ?? ({})

    readonly property int collapsedWidth: island.collapsedWidth ?? 168
    readonly property int collapsedHeight: island.collapsedHeight ?? 32
    readonly property int expandedWidth: island.expandedWidth ?? 460
    readonly property int maxWidth: island.maxWidth ?? 620
    readonly property string position: island.position ?? "top-center"
    readonly property bool atBottom: position.startsWith("bottom")
    /** "notch" sits flush against the screen edge, "pill" floats below it. */
    readonly property string shape: island.shape ?? "notch"
    readonly property int sideMargin: island.sideMargin ?? 24
    readonly property bool alwaysVisible: island.alwaysVisible ?? true

    /** "hidden" only wins when the island is allowed to disappear entirely. */
    readonly property string idleMode: {
        const configured = island.idleMode ?? "auto"
        return configured === "hidden" && alwaysVisible ? "notch" : configured
    }

    /** The assistant needs both its own switch and the module switch. */
    readonly property bool aiEnabled: (modules.ai ?? true) && (ai.enabled ?? true)
    readonly property int aiLongPress: ai.longPressMs ?? 450
    /**
     * The island's usual ceiling is meant for a media dashboard; a conversation
     * needs more room than that, so the assistant carries its own.
     */
    readonly property int maxAiWidth: Math.max(maxWidth, ai.panelWidth ?? 560)

    readonly property bool gooey: effects.gooey ?? true
    readonly property real gooeyStrength: effects.gooeyStrength ?? 0.62
    readonly property real spring: effects.spring ?? 4.2
    readonly property real damping: effects.damping ?? 0.36
    readonly property real animationScale: effects.animationScale ?? 1.0

    readonly property int osdTimeout: osd.timeout ?? 1700
    readonly property int notificationTimeout: notifications.timeout ?? 5000
    readonly property int mediaPeekDuration: media.peekDuration ?? 4200

    readonly property bool lyricsInIsland: (lyrics.enabled ?? true) && (lyrics.showInIsland ?? true)
                                           && (modules.lyrics ?? true)
    readonly property bool lyricsInExpanded: (lyrics.enabled ?? true) && (lyrics.showInExpanded ?? true)
                                             && (modules.lyrics ?? true)

    /** Ordered list of transport button IDs to show. Falls back to the classic three. */
    readonly property var transportButtons: {
        const buttons = media.transportButtons
        if (Array.isArray(buttons) && buttons.length > 0) return buttons
        return ["previous", "playPause", "next"]
    }

    /**
     * Read any dotted key reactively. Because it walks `data`, every binding
     * that calls it re-evaluates when the config changes - which is what lets
     * the settings window bind straight to the file it is editing.
     */
    function get(key, fallback) {
        let cursor = data
        for (const part of key.split(".")) {
            if (cursor === undefined || cursor === null) {
                return fallback
            }
            cursor = cursor[part]
        }
        return cursor === undefined ? fallback : cursor
    }

    function set(key, value) {
        App.config.setValue(key, value)
    }

    function ms(base) {
        return Math.max(1, Math.round(base * animationScale))
    }
}
