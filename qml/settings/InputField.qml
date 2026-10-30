// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A single-line text box that commits on Enter or on losing the focus. */
Rectangle {
    id: field

    property string value: ""
    property string placeholder: ""
    property var validator: null
    property alias inputMethodHints: input.inputMethodHints
    property alias horizontalAlignment: input.horizontalAlignment

    signal committed(string value)

    implicitWidth: 220
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: Skin.field
    border.width: input.activeFocus ? 2 : 0
    border.color: Qt.alpha(Skin.accent, 0.8)

    onValueChanged: if (!input.activeFocus) {
        input.text = value
    }

    Component.onCompleted: input.text = value

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        color: Skin.text
        selectionColor: Skin.accent
        selectedTextColor: Skin.accentText
        font.pixelSize: 12
        validator: field.validator
        selectByMouse: true
        onEditingFinished: field.committed(text)

        Keys.onEscapePressed: {
            text = field.value
            focus = false
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: input.horizontalAlignment
        text: field.placeholder
        visible: input.text.length === 0 && !input.activeFocus
        color: Skin.faint
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}
