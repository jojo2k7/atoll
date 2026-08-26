// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Content")
    description: qsTr("What the island shows when nothing is happening, and which sources it listens to.")

    SettingsGroup {
        title: qsTr("At rest")

        ChoiceSetting {
            label: qsTr("Resting state")
            description: qsTr("\"Auto\" shows the clock, the cover of whatever is playing, and a dot for anything waiting.")
            key: "island.idleMode"
            defaultValue: "auto"
            options: [
                { value: "auto", label: qsTr("Auto") },
                { value: "clock", label: qsTr("Clock") },
                { value: "notch", label: qsTr("Bare notch") },
                { value: "hidden", label: qsTr("Hidden") }
            ]
        }

        BoolSetting {
            label: qsTr("Always show something")
            description: qsTr("Keeps the island on screen even in \"hidden\" mode, so there is always somewhere to click.")
            key: "island.alwaysVisible"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Pill contents")

        BoolSetting {
            label: qsTr("Clock")
            key: "idle.showClock"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Day and date")
            description: qsTr("The date next to the time, in the format set further down this page.")
            key: "idle.showDate"
            defaultValue: false
        }

        BoolSetting {
            label: qsTr("Cover of what is playing")
            description: qsTr("While music plays. \"Show a cover on the resting island\" under Media also has to be on.")
            key: "idle.showMediaBadge"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Notification dot")
            description: qsTr("The pulsing accent dot while notifications are waiting.")
            key: "idle.showNotificationDot"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Low battery dot")
            key: "idle.showBatteryDot"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Privacy indicators")
            description: qsTr("One dot, blended orange, red and violet while an application is using the microphone, the camera or your screen.")
            key: "idle.showPrivacyIndicators"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Next calendar event")
            description: qsTr("\"Show in resting island\" under Calendar also has to be on.")
            key: "idle.showCalendarHint"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Sources")

        BoolSetting {
            label: qsTr("Volume and brightness")
            description: qsTr("Mirrors Plasma's own OSD.")
            key: "modules.osd"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Notifications")
            description: qsTr("Mirrors your notification daemon; it does not replace it.")
            key: "modules.notifications"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Media")
            key: "modules.media"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Sharing")
            description: qsTr("Files dropped on the island, and files offered by nearby devices.")
            key: "modules.sharing"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Lyrics")
            key: "modules.lyrics"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Battery")
            key: "modules.battery"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Bluetooth")
            description: qsTr("Adapter power and device connections, managed from the dashboard.")
            key: "modules.bluetooth"
            defaultValue: true
        }
        BoolSetting {
            label: qsTr("Privacy indicators")
            description: qsTr("Watches PipeWire for camera, microphone and screen capture. Without it the dots never light.")
            key: "modules.privacy"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Bluetooth device panel")
            description: qsTr("Adds a collapsible panel of paired devices to the dashboard; click its header to fold or unfold it.")
            key: "bluetooth.showInExpanded"
            defaultValue: false
            enabled: Cfg.get("modules.bluetooth", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Spectrum")
            description: qsTr("Needs cava to show a real spectrum; without it the bars are decorative.")
            key: "modules.visualizer"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Clock")
            key: "modules.clock"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Calendar")
            description: qsTr("Upcoming events in the dashboard and resting island.")
            key: "modules.calendar"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Lock screen")

        SettingRow {
            label: qsTr("Compositor support")
            description: App.shell.lockScreenSupported
                         ? qsTr("Available. KWin lets the island stay up while the session is locked.")
                         : qsTr("Unavailable: %1. The island will disappear with the rest of the session.")
                           .arg(App.shell.lockScreenReason)
        }

        BoolSetting {
            label: qsTr("Keep the island on the lock screen")
            description: qsTr("Takes effect the next time the island starts, because the permission has to be asked for before the window is shown.")
            key: "lockScreen.enabled"
            defaultValue: true
            enabled: App.shell.lockScreenSupported
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Show what is playing")
            description: qsTr("Cover, title and artist stay readable while locked.")
            key: "lockScreen.showMedia"
            defaultValue: true
            enabled: Cfg.get("lockScreen.enabled", true) && App.shell.lockScreenSupported
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Show notifications")
            description: qsTr("Off by default: anybody walking past a locked machine can read whatever the island shows.")
            key: "lockScreen.showNotifications"
            defaultValue: false
            enabled: Cfg.get("lockScreen.enabled", true) && App.shell.lockScreenSupported
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Allow the dashboard")
            description: qsTr("Lets the island be expanded while locked, which exposes the notification history and the transport controls.")
            key: "lockScreen.allowExpanding"
            defaultValue: false
            enabled: Cfg.get("lockScreen.enabled", true) && App.shell.lockScreenSupported
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Clock")

        TextSetting {
            label: qsTr("Time format")
            description: qsTr("Qt date format, e.g. HH:mm or h:mm ap.")
            key: "clock.timeFormat"
            defaultValue: "HH:mm"
            placeholder: "HH:mm"
        }

        TextSetting {
            label: qsTr("Date format")
            key: "clock.dateFormat"
            defaultValue: "ddd d MMM"
            placeholder: "ddd d MMM"
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Volume and brightness")

        NumberSetting {
            label: qsTr("On screen for")
            key: "osd.timeout"
            defaultValue: 1700
            from: 300
            to: 10000
            step: 100
            suffix: " ms"
        }

        BoolSetting {
            label: qsTr("Include media player volume")
            key: "osd.showMediaPlayerVolume"
            defaultValue: true
            last: true
        }
    }
}
