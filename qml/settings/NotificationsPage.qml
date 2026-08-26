// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Notifications")
    description: qsTr("Atoll mirrors the notifications your daemon already shows. Silencing it here does not silence your daemon.")

    SettingsGroup {
        title: qsTr("On the island")

        NumberSetting {
            label: qsTr("On screen for")
            key: "notifications.timeout"
            defaultValue: 5000
            from: 500
            to: 60000
            step: 500
            suffix: " ms"
        }

        BoolSetting {
            label: qsTr("Keep urgent ones open")
            description: qsTr("Critical notifications stay until they are acknowledged.")
            key: "notifications.criticalStaysOpen"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Body lines")
            key: "notifications.maxBodyLines"
            defaultValue: 3
            from: 0
            to: 10
        }

        BoolSetting {
            label: qsTr("Show actions")
            description: qsTr("Buttons a notification offers. As a bus observer, Atoll can only re-broadcast them; some senders ignore that.")
            key: "notifications.showActions"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Open the app when clicked")
            description: qsTr("Clicking a notification raises the application that sent it, or starts one if none is running. The default action still goes to the app first, so on Plasma's daemon chat clients land you in the conversation itself.")
            key: "notifications.openOnClick"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Do not disturb")
            description: qsTr("The island collects notifications silently. The same switch sits in the dashboard.")
            key: "notifications.dnd"
            defaultValue: false
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Filtering")

        ListSetting {
            label: qsTr("Ignored apps")
            description: qsTr("Application names as they appear in the notification, comma separated.")
            key: "notifications.ignoredApps"
            placeholder: qsTr("Spotify, Steam")
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Plumbing")

        BoolSetting {
            label: qsTr("Track notification ids")
            description: qsTr("Pairs each observed call with its reply so notifications can be closed from the island. Turning this off makes the bus tap quieter but the close button stops working. Takes effect on restart.")
            key: "notifications.trackIds"
            defaultValue: true
            last: true
        }
    }
}
