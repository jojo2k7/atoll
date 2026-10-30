// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A segmented control: one track, with a thumb that slides to the value. */
Rectangle {
    id: control

    /** [{ value: "top", label: "Top" }] */
    property var options: []
    property var current: ""

    signal picked(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === current) {
                return i
            }
        }
        return -1
    }

    implicitWidth: segments.implicitWidth + 4
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: Skin.field

    Rectangle {
        readonly property Item target: control.currentIndex >= 0 ? segments.children[control.currentIndex] ?? null : null

        visible: target !== null
        x: 2 + (target ? target.x : 0)
        y: 2
        width: target ? target.width : 0
        height: parent.height - 4
        radius: Skin.radius - 2
        color: Skin.controlHover

        Behavior on x {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
        Behavior on width {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
    }

    Row {
        id: segments
        x: 2
        y: 2

        Repeater {
            model: control.options

            delegate: Item {
                required property var modelData
                required property int index

                readonly property bool active: index === control.currentIndex

                width: Math.max(52, caption.implicitWidth + 24)
                height: control.height - 4

                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: parent.height * 0.5
                    color: Skin.line
                    visible: index < control.options.length - 1 && !parent.active
                             && index + 1 !== control.currentIndex
                }

                Text {
                    id: caption
                    anchors.centerIn: parent
                    text: modelData.label
                    color: parent.active ? Skin.text : Skin.muted
                    font.pixelSize: 12
                    font.weight: parent.active ? Font.DemiBold : Font.Normal
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: control.picked(modelData.value)
                }
            }
        }
    }
}
