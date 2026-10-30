// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    id: page

    title: qsTr("About")
    description: qsTr("Atoll %1 - a dynamic island for KDE Plasma.").arg(App.version)

    property bool confirmingReset: false
    property bool islandUp: App.islandRunning()

    SettingsGroup {
        title: qsTr("Status")

        SettingRow {
            label: qsTr("Islands")
            description: App.shell.targets.length > 0
                         ? App.shell.targets.join(", ")
                         : qsTr("No output matches the current selection.")
        }

        SettingRow {
            label: qsTr("Island process")
            description: page.islandUp
                         ? qsTr("Running. Every change on this page reaches it as you make it.")
                         : qsTr("Not running, so nothing is showing your settings yet.")
            last: true

            PushButton {
                text: page.islandUp ? qsTr("Restart") : qsTr("Start")
                onClicked: {
                    App.startIsland()
                    page.islandUp = true
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Configuration")

        SettingRow {
            label: qsTr("Config file")
            description: App.config.path

            PushButton {
                text: qsTr("Open")
                onClicked: App.openUrl("file://" + App.config.path)
            }
        }

        SettingRow {
            label: qsTr("Reset everything")
            description: qsTr("Puts every setting back to how Atoll ships.")
            last: true

            PushButton {
                text: page.confirmingReset ? qsTr("Really reset?") : qsTr("Reset")
                destructive: true
                onClicked: {
                    if (page.confirmingReset) {
                        App.config.resetAll()
                        page.confirmingReset = false
                        confirmTimeout.stop()
                    } else {
                        page.confirmingReset = true
                        confirmTimeout.restart()
                    }
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Scripting")

        SettingRow {
            label: qsTr("D-Bus")
            description: "org.atoll.Atoll on /Atoll - action, expand, collapse, toggle, showText, showProgress, share, office, settings, reloadConfig, quit."
        }

        SettingRow {
            label: qsTr("Command line")
            description: "atollctl text drive-harddisk \"Backup finished\"\natollctl progress cloud-upload 64 \"Uploading\"\natollctl settings"
            last: true
        }
    }

    Timer {
        interval: 2000
        running: true
        repeat: true
        onTriggered: page.islandUp = App.islandRunning()
    }

    Timer {
        id: confirmTimeout
        interval: 4000
        onTriggered: page.confirmingReset = false
    }
}
