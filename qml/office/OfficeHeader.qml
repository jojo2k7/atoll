// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The strip above the scene: one tab per session, the counts of the session
 * on screen (working / resting / waiting, tool calls, time) and the close
 * button.
 */
Item {
    id: header

    required property Item view

    property real now: Date.now()
    Timer {
        interval: 1000
        running: header.visible
        repeat: true
        onTriggered: header.now = Date.now()
    }

    Row {
        id: tabs
        x: 2
        y: 3
        spacing: 2

        PixelText {
            visible: App.office.sessions.length === 0
            y: 2
            x: 2
            text: qsTr("Claude Code office")
            color: OfficeArt.dim
        }

        Repeater {
            model: App.office.sessions.slice(0, 5)

            Item {
                id: tab
                required property var modelData
                readonly property bool current: modelData.id === App.office.currentSession
                readonly property string title: {
                    const p = modelData.project || "session"
                    return p.length > 14 ? p.slice(0, 13) + "…" : p
                }
                width: title.length * 6 + 14 + (modelData.waiting > 0 ? 4 : 0)
                height: 12

                NineSlice {
                    anchors.fill: parent
                    sheet: "ui_tab"
                    frame: tab.current ? "active" : "inactive"
                }
                Sprite {
                    sheet: "ui_led_dot"
                    frame: tab.modelData.waiting > 0 ? "warn" : (tab.current ? "ok" : "off")
                    px: 4
                    py: 5
                }
                PixelText {
                    x: 9
                    y: 3
                    text: tab.title
                    color: tab.current ? OfficeArt.text : OfficeArt.dim
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.office.selectSession(tab.modelData.id)
                }
            }
        }
    }

    Row {
        id: stats
        anchors.right: close.left
        anchors.rightMargin: 6
        y: 3
        spacing: 7
        visible: App.office.sessions.length > 0

        Repeater {
            model: [
                { icon: "bolt", value: App.office.working, color: OfficeArt.ok },
                { icon: "cat", value: App.office.resting, color: OfficeArt.info },
                { icon: "question", value: App.office.waiting, color: OfficeArt.warn },
                { icon: "terminal", value: App.office.toolCalls, color: OfficeArt.text },
                { icon: "clock", value: App.office.sessionStart > 0
                                        ? OfficeArt.fmtDuration(header.now - App.office.sessionStart) : "-",
                  color: OfficeArt.dim }
            ]
            Item {
                required property var modelData
                width: 11 + value.implicitWidth
                height: 10
                Sprite {
                    sheet: "icons"
                    frame: modelData.icon
                    px: 0
                    py: 1
                }
                PixelText {
                    id: value
                    x: 11
                    y: 2
                    text: String(modelData.value)
                    color: modelData.color
                }
            }
        }
    }

    SystemHud {
        view: header.view
        y: 3
        x: Math.round(Math.max(tabs.x + tabs.width + 6, Math.min((header.width - width) / 2, stats.x - width - 6)))
        visible: header.view.anyBusy && x + width <= stats.x - 2
    }

    PixelButton {
        id: close
        anchors.right: parent.right
        anchors.rightMargin: 1
        y: 1
        icon: "close"
        onClicked: App.office.hide()
    }
}
