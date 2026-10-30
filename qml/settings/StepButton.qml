// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** One half of the stepper next to a number field. */
Item {
    id: button

    property string glyph: "+"

    signal clicked()

    width: 26
    height: Skin.controlHeight

    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        radius: Skin.radius - 3
        color: hover.hovered ? Skin.controlHover : "transparent"
    }

    Text {
        anchors.centerIn: parent
        text: button.glyph
        color: hover.hovered ? Skin.text : Skin.muted
        font.pixelSize: 15
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: button.clicked()
    }
}
