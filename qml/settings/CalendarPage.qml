// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Calendar")
    description: qsTr("Upcoming events from Google Calendar, Apple iCloud, or any other calendar that exports an ICS feed.")

    SettingsGroup {
        title: qsTr("Your calendars")

        Repeater {
            model: Cfg.get("calendar.sources", []) || []

            delegate: Item {
                id: sourceRow
                required property var modelData
                required property int index

                width: parent ? parent.width : 0
                height: 52

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 0
                    anchors.rightMargin: 0
                    color: "transparent"
                }

                Row {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - removeBtn.width - 10
                        spacing: 1

                        Text {
                            width: parent.width
                            text: sourceRow.modelData.name || qsTr("Calendar")
                            color: Skin.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: sourceRow.modelData.url || ""
                            color: Skin.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    PushButton {
                        id: removeBtn
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Remove")
                        destructive: true
                        onClicked: {
                            const current = [...(Cfg.get("calendar.sources", []) || [])]
                            current.splice(sourceRow.index, 1)
                            Cfg.set("calendar.sources", current)
                            if (App.calendar) App.calendar.refresh()
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    height: 1
                    color: Skin.line
                }
            }
        }

        Item {
            visible: (Cfg.get("calendar.sources", []) || []).length === 0
            width: parent ? parent.width : 0
            height: 44

            Text {
                anchors.centerIn: parent
                text: qsTr("No calendars added yet.")
                color: Skin.muted
                font.pixelSize: 12
            }
        }

        SettingRow {
            label: qsTr("Add calendar")
            description: qsTr("ICS or webcal URL. Find it in your calendar app's sharing settings.")
            wide: true
            last: true

            Row {
                spacing: 8
                topPadding: 4
                bottomPadding: 8

                Rectangle {
                    width: 160
                    height: 32
                    radius: 9
                    color: Skin.field
                    border.width: 1
                    border.color: nameInput.activeFocus ? Skin.accent : Skin.line

                    TextInput {
                        id: nameInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        color: Skin.text
                        selectionColor: Skin.accent
                        selectedTextColor: "#101014"
                        font.pixelSize: 12
                        selectByMouse: true
                        Keys.onReturnPressed: addCalendar()
                    }

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Name (optional)")
                        visible: nameInput.text.length === 0 && !nameInput.activeFocus
                        color: Skin.muted
                        font.pixelSize: 12
                        opacity: 0.65
                    }
                }

                Rectangle {
                    width: 300
                    height: 32
                    radius: 9
                    color: Skin.field
                    border.width: 1
                    border.color: urlInput.activeFocus ? Skin.accent : Skin.line

                    TextInput {
                        id: urlInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        color: Skin.text
                        selectionColor: Skin.accent
                        selectedTextColor: "#101014"
                        font.pixelSize: 12
                        selectByMouse: true
                        inputMethodHints: Qt.ImhUrlCharactersOnly
                        Keys.onReturnPressed: addCalendar()
                    }

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("https:// or webcal://")
                        visible: urlInput.text.length === 0 && !urlInput.activeFocus
                        color: Skin.muted
                        font.pixelSize: 12
                        opacity: 0.65
                    }
                }

                PushButton {
                    text: qsTr("Add")
                    onClicked: addCalendar()
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Display")

        NumberSetting {
            label: qsTr("Look ahead")
            description: qsTr("Events starting within this many hours are shown in the dashboard.")
            key: "calendar.lookaheadHours"
            defaultValue: 24
            from: 1
            to: 168
            step: 1
            suffix: " h"
        }

        NumberSetting {
            label: qsTr("Fetch interval")
            description: qsTr("How often the calendar feeds are downloaded. Fetching very often may get you rate-limited by your provider.")
            key: "calendar.fetchIntervalMinutes"
            defaultValue: 15
            from: 1
            to: 1440
            step: 1
            suffix: " min"
        }

        NumberSetting {
            label: qsTr("Display refresh")
            description: qsTr("How often countdowns and event states are recomputed locally. This does not contact your provider.")
            key: "calendar.recomputeIntervalSeconds"
            defaultValue: 60
            from: 5
            to: 3600
            step: 5
            suffix: " s"
        }

        BoolSetting {
            label: qsTr("Show in resting island")
            description: qsTr("Displays the next event in the pill when a meeting is within 60 minutes.")
            key: "calendar.showInIdle"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Where to find the URL")

        SettingRow {
            label: qsTr("Google Calendar")
            description: qsTr("calendar.google.com → ⚙ Settings → pick a calendar → scroll to \"Integrate calendar\" → copy the \"Secret address in iCal format\".")
        }

        SettingRow {
            label: qsTr("Apple iCloud")
            description: qsTr("Calendar app → tap ⓘ next to a calendar → Share Calendar → enable Public Calendar → Copy Link. The link starts with webcal://.")
            last: true
        }
    }

    function addCalendar() {
        const url = urlInput.text.trim()
        if (!url) return
        const name = nameInput.text.trim() || qsTr("Calendar")
        const sources = [...(Cfg.get("calendar.sources", []) || [])]
        sources.push({ name: name, url: url })
        Cfg.set("calendar.sources", sources)
        nameInput.text = ""
        urlInput.text = ""
        if (App.calendar) App.calendar.refresh()
    }
}
