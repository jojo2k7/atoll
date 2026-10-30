// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Which outputs get an island.
 *
 * "Main monitor" is stored as the literal "primary" rather than as the name of
 * whatever is primary today, so the island keeps following the main monitor
 * when the layout changes.
 */
Column {
    id: picker

    readonly property var screens: Cfg.get("island.screens", ["primary"])
    readonly property string mode: {
        if (screens.indexOf("all") >= 0) {
            return "all"
        }
        if (screens.length === 1 && screens[0] === "primary") {
            return "primary"
        }
        return "custom"
    }

    spacing: 12

    function toggle(name, on) {
        let next = picker.screens.filter(entry => entry !== "all" && entry !== "primary" && entry !== name)
        if (on) {
            next.push(name)
        }
        Cfg.set("island.screens", next.length > 0 ? next : ["primary"])
    }

    Segmented {
        options: [
            { value: "primary", label: qsTr("Main monitor") },
            { value: "all", label: qsTr("All monitors") },
            { value: "custom", label: qsTr("Pick") }
        ]
        current: picker.mode
        onPicked: value => {
            if (value === "primary") {
                Cfg.set("island.screens", ["primary"])
            } else if (value === "all") {
                Cfg.set("island.screens", ["all"])
            } else {
                Cfg.set("island.screens", [App.shell.primaryScreen])
            }
        }
    }

    Column {
        width: parent.width
        spacing: 0

        Repeater {
            model: App.shell.screens

            delegate: Rectangle {
                required property var modelData

                readonly property bool selected: picker.mode === "all"
                                                 || picker.screens.indexOf(modelData.name) >= 0
                                                 || (picker.mode === "primary" && modelData.primary)

                width: picker.width
                height: 46
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Skin.line
                }

                IconTile {
                    id: screenTile
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    size: 26
                    glyph: "placement"
                    tint: App.shell.targets.indexOf(modelData.name) >= 0 ? Skin.accent : Skin.control
                }

                Column {
                    anchors.left: screenTile.right
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        text: modelData.name + (modelData.primary ? qsTr(" · main monitor") : "")
                        color: Skin.text
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: {
                            const size = modelData.width + " × " + modelData.height
                            const make = [modelData.manufacturer, modelData.model].filter(part => !!part).join(" ")
                            return make.length > 0 ? size + " · " + make : size
                        }
                        color: Skin.muted
                        font.pixelSize: 10
                    }
                }

                ToggleSwitch {
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: picker.mode === "custom"
                    opacity: picker.mode === "custom" ? 1 : 0.35
                    checked: parent.selected
                    onToggled: value => picker.toggle(modelData.name, value)
                }
            }
        }
    }
}
