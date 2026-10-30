// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** The small sign shown while no Claude Code session is live. */
Item {
    id: sign

    property int cx: 0
    property int topY: 0

    width: label.implicitWidth + 24
    height: 17
    x: Math.round(cx - width / 2)
    y: topY

    NineSlice {
        anchors.fill: parent
        sheet: "ui_panel_dark"
    }

    Sprite {
        sheet: "icons"
        frame: "power"
        px: 6
        py: 4
    }

    PixelText {
        id: label
        x: 18
        y: 5
        text: qsTr("No Claude Code session")
        color: OfficeArt.dim
    }
}
