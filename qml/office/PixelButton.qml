// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A UI-kit button (`ui_button` / `ui_button_accent`), optional icon, 14 px high. */
Item {
    id: button

    property string text: ""
    property string icon: ""
    property bool accent: false
    property bool highlighted: false
    signal clicked()

    implicitWidth: (text.length > 0 ? label.implicitWidth + 12 : 4) + (icon.length > 0 ? 11 : 0)
    implicitHeight: 14
    width: implicitWidth
    height: implicitHeight

    NineSlice {
        anchors.fill: parent
        sheet: button.accent ? "ui_button_accent" : "ui_button"
        frame: mouse.pressed ? "pressed" : (mouse.containsMouse || button.highlighted ? "hover" : "normal")
    }

    Sprite {
        visible: button.icon.length > 0
        sheet: "icons"
        frame: button.icon
        px: button.text.length > 0 ? 5 : 3
        py: mouse.pressed ? 3 : 2
    }

    PixelText {
        id: label
        x: (button.icon.length > 0 ? 16 : 6)
        y: mouse.pressed ? 4 : 3
        text: button.text
        color: button.accent ? "#2a1a14" : OfficeArt.text
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }
}
