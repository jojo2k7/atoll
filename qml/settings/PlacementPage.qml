// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Placement")
    description: qsTr("Which screens the island lives on, where it sits, and what it is allowed to cover.")

    SettingsGroup {
        title: qsTr("Shape")

        ChoiceSetting {
            label: qsTr("Island shape")
            description: qsTr("A notch grows out of the screen edge with square corners against it, the way a MacBook's does. A pill is rounded all round and floats below the edge.")
            key: "island.shape"
            defaultValue: "notch"
            options: [
                { value: "notch", label: qsTr("Notch") },
                { value: "pill", label: qsTr("Pill") }
            ]
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Monitors")

        SettingRow {
            label: qsTr("Show the island on")
            description: qsTr("Outputs with an island are outlined. \"Main monitor\" follows the primary output, so the island moves with it when the layout changes.")
            wide: true
            last: true

            ScreenPicker {
                width: parent.width
            }
        }
    }

    SettingsGroup {
        title: qsTr("Position")

        SettingRow {
            label: qsTr("Corner")
            description: qsTr("Where on the output the island rests.")
            wide: true

            PositionPicker {
                value: Cfg.get("island.position", "top-center")
                onPicked: value => Cfg.set("island.position", value)
            }
        }

        NumberSetting {
            label: qsTr("Edge margin")
            description: Cfg.shape === "notch"
                         ? qsTr("Only for the pill shape - a notch always sits flush against the edge.")
                         : qsTr("Distance from the top or bottom edge of the screen.")
            key: "island.edgeMargin"
            defaultValue: 0
            from: 0
            to: 400
            suffix: " px"
            enabled: Cfg.shape !== "notch"
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Side margin")
            description: qsTr("Only used when the island is aligned left or right.")
            key: "island.sideMargin"
            defaultValue: 24
            from: 0
            to: 600
            suffix: " px"
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Stacking")

        BoolSetting {
            label: qsTr("Draw over Plasma's own elements")
            description: qsTr("Lets the island sit on top of panels, docks and the desktop instead of being pushed below them.")
            key: "island.overlapPanels"
            defaultValue: true
        }

        ChoiceSetting {
            label: qsTr("Layer")
            description: qsTr("How high the surface sits in the compositor's stack. \"Overlay\" is above panels and full-screen windows.")
            key: "island.layer"
            defaultValue: "overlay"
            options: [
                { value: "background", label: qsTr("Background") },
                { value: "bottom", label: qsTr("Bottom") },
                { value: "top", label: qsTr("Top") },
                { value: "overlay", label: qsTr("Overlay") }
            ]
        }

        NumberSetting {
            label: qsTr("Reserved space")
            description: qsTr("Only when the island does not draw over panels: how much room the compositor keeps free for it.")
            key: "island.exclusiveZone"
            defaultValue: 0
            from: -1
            to: 600
            enabled: !Cfg.get("island.overlapPanels", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Size")

        BoolSetting {
            label: qsTr("Fit collapsed content")
            description: qsTr("Only use the space needed by the clock, icons or focus status.")
            key: "island.fitCollapsedContent"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Collapsed side padding")
            key: "island.collapsedPadding"
            defaultValue: 12
            from: 4
            to: 32
            step: 1
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Collapsed minimum width")
            enabled: !Cfg.fitCollapsedContent
            opacity: enabled ? 1 : 0.45
            key: "island.collapsedWidth"
            defaultValue: 168
            from: 40
            to: 900
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Collapsed height")
            key: "island.collapsedHeight"
            defaultValue: 32
            from: 16
            to: 200
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Expanded width")
            key: "island.expandedWidth"
            defaultValue: 460
            from: 200
            to: 1200
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Maximum width")
            description: qsTr("The island never grows past this, whatever it is showing.")
            key: "island.maxWidth"
            defaultValue: 620
            from: 200
            to: 1600
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Corner radius")
            description: qsTr("0 keeps the pill shape and lets the dashboard round its own corners.")
            key: "island.cornerRadius"
            defaultValue: 0
            from: 0
            to: 60
            suffix: " px"
        }

        NumberSetting {
            label: qsTr("Surface height")
            description: qsTr("The invisible canvas the island morphs inside. It is a floor: a dashboard that needs more room gets it.")
            key: "island.surfaceHeight"
            defaultValue: 700
            from: 240
            to: 1600
            suffix: " px"
            last: true
        }
    }
}
