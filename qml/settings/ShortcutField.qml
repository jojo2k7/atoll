// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Records a key combination. Click, press the keys, done; Backspace clears,
 * Escape gives up. The value is Qt's portable text ("Meta+Shift+O"), which is
 * what controls.shortcuts stores.
 */
Rectangle {
    id: control

    property string value: ""
    property bool recording: false

    signal committed(string value)

    function keyName(key, text) {
        const named = {
            [Qt.Key_Space]: "Space", [Qt.Key_Return]: "Return", [Qt.Key_Enter]: "Enter",
            [Qt.Key_Tab]: "Tab", [Qt.Key_Up]: "Up", [Qt.Key_Down]: "Down", [Qt.Key_Left]: "Left",
            [Qt.Key_Right]: "Right", [Qt.Key_Home]: "Home", [Qt.Key_End]: "End",
            [Qt.Key_PageUp]: "PgUp", [Qt.Key_PageDown]: "PgDown", [Qt.Key_Insert]: "Ins",
            [Qt.Key_Delete]: "Del", [Qt.Key_Print]: "Print"
        }
        if (named[key] !== undefined) {
            return named[key]
        }
        if (key >= Qt.Key_F1 && key <= Qt.Key_F24) {
            return "F" + (key - Qt.Key_F1 + 1)
        }
        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
            return String.fromCharCode(key)
        }
        if (key >= Qt.Key_0 && key <= Qt.Key_9) {
            return String.fromCharCode(key)
        }
        return text.length === 1 ? text.toUpperCase() : ""
    }

    /** "Meta+Shift+O" spelled the way the keys look. */
    function pretty(sequence) {
        if (sequence.length === 0) {
            return ""
        }
        return sequence.split("+").map(part => ({ Meta: "Super", Ctrl: "Ctrl", Alt: "Alt", Shift: "Shift" })[part] ?? part)
                       .join(" + ")
    }

    implicitWidth: Math.max(130, caption.implicitWidth + 40)
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: control.recording ? Qt.alpha(Skin.accent, 0.16) : (hover.hovered ? Skin.controlHover : Skin.field)
    border.width: control.recording ? 2 : 0
    border.color: Qt.alpha(Skin.accent, 0.8)

    Text {
        id: caption
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.verticalCenter: parent.verticalCenter
        text: control.recording ? qsTr("Press keys…")
                                : (control.value.length > 0 ? control.pretty(control.value) : qsTr("Record shortcut"))
        color: control.recording ? Skin.accent : (control.value.length > 0 ? Skin.text : Skin.faint)
        font.pixelSize: 12
        font.weight: control.value.length > 0 && !control.recording ? Font.Medium : Font.Normal
    }

    Image {
        anchors.right: parent.right
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        sourceSize: Qt.size(28, 28)
        source: Skin.icon("clear")
        visible: control.value.length > 0 && !control.recording
        opacity: clearHover.hovered ? 1 : 0.6

        HoverHandler {
            id: clearHover
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: control.committed("")
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: {
            control.recording = true
            control.forceActiveFocus()
        }
    }

    onActiveFocusChanged: if (!activeFocus) {
        recording = false
    }

    Keys.onPressed: event => {
        if (!control.recording) {
            return
        }
        event.accepted = true
        if (event.key === Qt.Key_Escape) {
            control.recording = false
            return
        }
        if (event.key === Qt.Key_Backspace && event.modifiers === Qt.NoModifier) {
            control.recording = false
            control.committed("")
            return
        }
        const key = control.keyName(event.key, event.text)
        if (key.length === 0) {
            return // a modifier on its own; wait for the rest
        }
        const parts = []
        if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
        if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
        if (event.modifiers & Qt.AltModifier) parts.push("Alt")
        if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
        parts.push(key)
        control.recording = false
        control.committed(parts.join("+"))
    }
}
