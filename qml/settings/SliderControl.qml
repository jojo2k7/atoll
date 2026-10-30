// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A slider with its value spelled out, for the continuous appearance knobs. */
Item {
    id: control

    property real value: 0
    property real from: 0
    property real to: 1
    property real step: 0.01
    property int decimals: 2
    property string suffix: ""

    /** Fires while dragging; the config write is debounced anyway. */
    signal moved(real value)

    implicitWidth: 240
    implicitHeight: 30

    readonly property real ratio: to > from ? Math.max(0, Math.min(1, (value - from) / (to - from))) : 0

    function valueAt(x) {
        const raw = control.from + (x / Math.max(1, groove.width)) * (control.to - control.from)
        const snapped = Math.round(raw / control.step) * control.step
        const bounded = Math.max(control.from, Math.min(control.to, snapped))
        return Number(bounded.toFixed(Math.max(0, control.decimals)))
    }

    Item {
        id: groove
        anchors.left: parent.left
        anchors.right: readout.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 4
            radius: 2
            color: Skin.control

            Rectangle {
                width: parent.width * control.ratio
                height: parent.height
                radius: parent.radius
                color: Skin.accent
            }
        }

        Rectangle {
            x: (parent.width - width) * control.ratio
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 8
            color: "#ffffff"
            scale: drag.pressed ? 1.15 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: 120
                }
            }
        }

        MouseArea {
            id: drag
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onPressed: mouse => control.moved(control.valueAt(mouse.x))
            onPositionChanged: mouse => {
                if (pressed) {
                    control.moved(control.valueAt(mouse.x))
                }
            }
        }
    }

    Text {
        id: readout
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 52
        horizontalAlignment: Text.AlignRight
        text: (control.decimals > 0 ? control.value.toFixed(control.decimals) : Math.round(control.value))
              + control.suffix
        color: Skin.muted
        font.pixelSize: 12
    }
}
