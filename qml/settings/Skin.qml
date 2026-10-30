// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * The settings window's own palette.
 *
 * It deliberately does not follow `appearance.*`: those keys describe the
 * island, and a settings page that repaints itself while you drag its own
 * colour pickers is impossible to aim. Only the accent is shared, so the
 * window still feels like the island it configures.
 */
QtObject {
    readonly property color window: "#141416"
    readonly property color sidebar: "#0e0e10"
    readonly property color card: "#1c1c1f"
    readonly property color cardHover: "#26262a"
    readonly property color field: "#202024"
    readonly property color control: "#2c2c31"
    readonly property color controlHover: "#36363c"
    readonly property color selection: "#14ffffff"
    readonly property color text: "#f5f5f7"
    readonly property color muted: "#9a9aa2"
    readonly property color faint: "#5f5f68"
    readonly property color line: "#12ffffff"
    readonly property color accent: Theme.accent
    readonly property color accentText: "#0c0c0e"
    readonly property color critical: "#ff5f57"

    readonly property int radius: 8
    readonly property int pagePadding: 36
    readonly property int rowHeight: 46
    readonly property int controlHeight: 28

    /** Where the bundled glyphs live inside the QML module. */
    function icon(name) {
        return "qrc:/qt/qml/Atoll/data/icons/settings/" + name + ".svg"
    }
}
