// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A set of chips, each switching one entry of a list key on or off. The list
 * is always written in the order of `options`, so what is on screen stays in
 * a predictable order however the chips were clicked.
 */
SettingRow {
    id: setting

    property string key: ""
    /** [{ value: "next", label: "Next" }] */
    property var options: []
    property var defaultValue: []

    readonly property var chosen: Cfg.get(setting.key, setting.defaultValue) || []

    wide: true

    Flow {
        width: parent.width
        spacing: 6

        Repeater {
            model: setting.options

            delegate: Rectangle {
                required property var modelData

                readonly property bool on: setting.chosen.indexOf(modelData.value) >= 0

                width: chip.implicitWidth + 26
                height: Skin.controlHeight
                radius: height / 2
                color: on ? Qt.alpha(Skin.accent, 0.2) : (hover.hovered ? Skin.controlHover : Skin.control)
                border.width: on ? 1 : 0
                border.color: Qt.alpha(Skin.accent, 0.7)

                Text {
                    id: chip
                    anchors.centerIn: parent
                    text: modelData.label
                    color: parent.on ? Skin.text : Skin.muted
                    font.pixelSize: 12
                    font.weight: parent.on ? Font.DemiBold : Font.Normal
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: {
                        const wanted = setting.options.map(o => o.value).filter(v => v === modelData.value
                                                                                        ? !parent.on
                                                                                        : setting.chosen.indexOf(v) >= 0)
                        Cfg.set(setting.key, wanted)
                    }
                }
            }
        }
    }
}
