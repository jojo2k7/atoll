// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** An ordinary button, for the few actions a settings page needs. */
Rectangle {
    id: button

    property string text: ""
    property bool destructive: false
    property bool prominent: false

    signal clicked()

    implicitWidth: Math.max(80, caption.implicitWidth + 28)
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: button.prominent ? (hover.hovered ? Qt.lighter(Skin.accent, 1.1) : Skin.accent)
                            : (hover.hovered ? Skin.controlHover : Skin.control)

    Behavior on color {
        ColorAnimation {
            duration: 120
        }
    }

    Text {
        id: caption
        anchors.centerIn: parent
        text: button.text
        color: button.destructive ? Skin.critical : (button.prominent ? Skin.accentText : Skin.text)
        font.pixelSize: 12
        font.weight: button.prominent ? Font.DemiBold : Font.Normal
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: button.clicked()
    }
}
