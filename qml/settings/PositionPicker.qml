// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Where on the screen the island sits, picked on a picture of a screen rather
 * than from a list of compass directions.
 */
Item {
    id: picker

    property string value: "top-center"

    signal picked(string value)

    implicitWidth: 220
    implicitHeight: 124

    Rectangle {
        id: screen
        anchors.fill: parent
        radius: 12
        color: Skin.field

        Grid {
            anchors.fill: parent
            anchors.margins: 10
            columns: 3
            rows: 2
            spacing: 0

            Repeater {
                model: [
                    { value: "top-left", row: 0 },
                    { value: "top-center", row: 0 },
                    { value: "top-right", row: 0 },
                    { value: "bottom-left", row: 1 },
                    { value: "bottom-center", row: 1 },
                    { value: "bottom-right", row: 1 }
                ]

                delegate: Item {
                    required property var modelData

                    readonly property bool active: modelData.value === picker.value

                    width: (screen.width - 20) / 3
                    height: (screen.height - 20) / 2

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: modelData.row === 0 ? parent.top : undefined
                        anchors.bottom: modelData.row === 1 ? parent.bottom : undefined
                        width: parent.width - 14
                        height: 12
                        radius: 6
                        color: parent.active ? Skin.accent : (hover.hovered ? Skin.controlHover : Skin.control)

                        Behavior on color {
                            ColorAnimation {
                                duration: 140
                            }
                        }
                    }

                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: picker.picked(modelData.value)
                    }
                }
            }
        }
    }
}
