// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The small load readout in the header strip while some agent runs a long
 * shell command: who and for how long, with CPU and memory meters. Click it
 * for the full system panel.
 */
Item {
    id: hud

    required property Item view

    readonly property var sys: App.office.system
    readonly property var busyAgent: App.office.agents.find(a => a.busy)
    readonly property real ramFrac: sys.memTotal > 0 ? sys.memUsed / sys.memTotal : 0
    property real now: Date.now()

    Timer {
        interval: 500
        running: hud.visible
        repeat: true
        onTriggered: hud.now = Date.now()
    }

    width: row.width + 10
    height: 12

    NineSlice {
        anchors.fill: parent
        sheet: "ui_tab"
        frame: mouse.containsMouse ? "active" : "inactive"
    }

    Row {
        id: row
        x: 4
        spacing: 5

        Item {
            width: 9 + 3 + who.implicitWidth
            height: 12
            Sprite {
                sheet: "icons"
                frame: "terminal"
                py: 2
            }
            PixelText {
                id: who
                x: 12
                y: 3
                text: hud.busyAgent ? hud.busyAgent.name + " " + OfficeArt.fmtDuration(hud.now - hud.busyAgent.runningSince) : ""
                color: OfficeArt.warn
            }
        }

        Repeater {
            model: [
                { icon: "cpu", frac: hud.sys.cpu / 100, label: Math.round(hud.sys.cpu) + "%" },
                { icon: "ram", frac: hud.ramFrac, label: OfficeArt.fmtBytes(hud.sys.memUsed) }
            ]
            Item {
                required property var modelData
                width: 9 + 3 + 26 + 3 + label.implicitWidth
                height: 12
                Sprite {
                    sheet: "icons"
                    frame: modelData.icon
                    py: 2
                }
                NineSlice {
                    x: 12
                    y: 3
                    width: 26
                    height: 7
                    sheet: "ui_bar_bg"
                }
                NineSlice {
                    x: 13
                    y: 4
                    width: Math.max(2, Math.round(24 * Math.min(1, modelData.frac)))
                    height: 5
                    sheet: "ui_bar_fill"
                    frame: modelData.frac > 0.85 ? "hot" : (modelData.frac > 0.6 ? "warn" : "ok")
                }
                PixelText {
                    id: label
                    x: 41
                    y: 3
                    text: modelData.label
                    color: OfficeArt.dim
                }
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: hud.view.openSystem(hud.busyAgent ? hud.busyAgent.pid : 0)
    }
}
