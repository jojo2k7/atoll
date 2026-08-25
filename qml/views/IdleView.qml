// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The resting state, and the one the island spends its day in - so it is never
 * empty. "auto" shows the clock, plus the cover of whatever is playing and a
 * dot for anything waiting; "notch" keeps the bare pill; "clock" is the time on
 * its own. Only "hidden" collapses it to nothing, and only when the island is
 * explicitly allowed to disappear.
 *
 * Which of those things appear at all is the user's call: every element gates
 * on its own switch under "idle.*" in the config.
 */
Item {
    id: view

    readonly property bool autoMode: Cfg.idleMode === "auto"
    readonly property bool clockAllowed: (autoMode || Cfg.idleMode === "clock") && (Cfg.modules.clock ?? true)
    readonly property bool showClock: clockAllowed && (Cfg.get("idle.showClock", true))
    /** The day and the date beside the time, using the configured date format. */
    readonly property bool showDate: clockAllowed && Cfg.get("idle.showDate", false)
    readonly property bool playing: App.media.active !== null && App.media.active.playing
                                    && (Cfg.modules.media ?? true)
    // "media.idleBadge" is the older spelling of the same switch; either off wins.
    readonly property bool showCover: autoMode && playing && (Cfg.media.idleBadge ?? true)
                                      && (Cfg.get("idle.showMediaBadge", true))
                                      && (!App.lock.locked || (Cfg.lockScreen.showMedia ?? true))
    readonly property bool hasUnread: App.notifications.count > 0 && !App.notifications.doNotDisturb
                                      && (Cfg.modules.notifications ?? true)
                                      && (Cfg.get("idle.showNotificationDot", true))
    readonly property bool lowBattery: (Cfg.modules.battery ?? true) && App.battery.present
                                       && App.battery.percent <= 15 && !App.battery.charging
                                       && (Cfg.get("idle.showBatteryDot", true))
    /**
     * A job the user sent away is still theirs, so the pill keeps a face on it.
     * This is the whole of "continue in background": no window, no taskbar
     * entry, just the island quietly getting on with it.
     */
    readonly property bool assistantBusy: Cfg.aiEnabled && App.ai.background && App.ai.busy

    readonly property bool showCalendarHint: autoMode
        && (Cfg.modules.calendar ?? true)
        && (Cfg.get("idle.showCalendarHint", true))
        && (Cfg.get("calendar.showInIdle", true))
        && App.calendar !== null
        && App.calendar !== undefined
        && App.calendar.hasEvents
        && (App.calendar.nextEvent.minutesUntil ?? 999) <= 60

    implicitHeight: Cfg.collapsedHeight
    implicitWidth: Cfg.idleMode === "hidden"
                   ? 0
                   : Math.max(Cfg.collapsedWidth,
                              content.implicitWidth + Math.max(28, Theme.edgeInset * 2))

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8

        BloubBot {
            anchors.verticalCenter: parent.verticalCenter
            visible: view.assistantBusy && Cfg.get("ai.avatar", true)
            size: 18
            mood: App.ai.state === "permission" ? "alert" : "working"
            // The assistant's own colour, not the album art's: it is the one
            // thing on the pill that has nothing to do with what is playing.
            bodyColor: App.ai.state === "permission" ? Theme.critical : Theme.assistantTint
        }

        AlbumArt {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            cornerRadius: width * 0.3
            visible: view.showCover
            source: view.playing ? App.media.active.artUrl : ""
            fallbackIcon: view.playing ? App.media.active.iconName : "media-optical-audio"
        }

        Text {
            visible: view.showClock
            anchors.verticalCenter: parent.verticalCenter
            text: App.clock.time
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(13)
            font.weight: Font.DemiBold
        }

        Text {
            visible: view.showDate
            anchors.verticalCenter: parent.verticalCenter
            text: App.clock.date
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(11)
            font.weight: Font.Medium
        }

        Text {
            visible: view.showCalendarHint
            anchors.verticalCenter: parent.verticalCenter
            width: 80
            text: {
                if (!App.calendar || !App.calendar.hasEvents) return ""
                const ev = App.calendar.nextEvent
                if (!ev || !ev.title) return ""
                const mins = ev.minutesUntil ?? 0
                if (mins < 1) return ev.title
                return ev.title + " · " + mins + " min"
            }
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(9)
            elide: Text.ElideRight
        }

        Rectangle {
            visible: view.hasUnread
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.accent

            SequentialAnimation on opacity {
                running: view.hasUnread
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutSine }
            }
        }

        Rectangle {
            visible: view.lowBattery
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.critical
        }
    }
}
