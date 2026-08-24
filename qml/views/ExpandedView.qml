// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The dashboard the island unfolds into on click: a full transport with a
 * seek bar, the notification backlog, and the handful of toggles worth having
 * one click away.
 *
 * When the calendar module is configured, a tab bar appears so the user can
 * switch between the dashboard ("Now") and a full view of today's events
 * ("Calendar").
 */
Item {
    id: view

    readonly property var player: App.media.active
    readonly property bool hasMedia: player !== null && (Cfg.modules.media ?? true)
    readonly property bool hasLyrics: hasMedia && Cfg.lyricsInExpanded
                                      && (App.lyrics.synced || App.lyrics.plain.length > 0
                                          || App.lyrics.state === "loading")
    readonly property bool calendarEnabled: Cfg.modules.calendar !== false

    // Which tab is active: 0 = Now, 1 = Calendar.
    property int selectedTab: 0
    // so each section can gate on a single boolean.
    readonly property bool showingCalendar: calendarEnabled && selectedTab === 1

    signal collapseRequested()

    implicitWidth: Cfg.expandedWidth
    // Content scrolls internally; the island is capped at 560px.
    implicitHeight: Math.min(560, scroller.contentHeight)

    WheelHandler {
        target: scroller
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentHeight: column.implicitHeight + 28
        contentWidth: width
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            // The corner curve decides the minimum, so no card in the dashboard
            // can end up half outside the black.
            readonly property int hMargin: Math.max(16, Theme.edgeInset)
            x: hMargin
            y: 14
            width: parent.width - 2 * hMargin
            spacing: 14

            // ---- header -------------------------------------------------------
            Item {
                width: parent.width
                height: 40

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Text {
                        text: App.clock.time
                        color: Theme.foreground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(22)
                        font.weight: Font.Light
                    }

                    Text {
                        text: App.clock.date
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(11)
                    }
                }

                Item {
                    visible: App.battery.present && (Cfg.modules.battery ?? true)
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 34
                    height: 34

                    ProgressRing {
                        anchors.fill: parent
                        value: App.battery.percent / 100
                        fillColor: App.battery.percent <= 15 && !App.battery.charging
                                   ? Theme.critical
                                   : (App.battery.charging ? Theme.positive : Theme.accent)
                    }

                    Text {
                        anchors.centerIn: parent
                        text: App.battery.percent
                        color: Theme.foreground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(10)
                        font.weight: Font.DemiBold
                    }
                }
            }

            // ---- tab bar (Now | Calendar) -------------------------------------
            // Only shown when the calendar service is configured.
            Rectangle {
                visible: view.calendarEnabled
                width: parent.width
                height: 30
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.06)

                Row {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3

                    Repeater {
                        model: [qsTr("Now"), qsTr("Calendar")]

                        delegate: Rectangle {
                            required property string modelData
                            required property int index

                            width: (parent.width - 3) / 2
                            height: parent.height
                            radius: 8
                            color: view.selectedTab === index
                                   ? Qt.rgba(1, 1, 1, 0.12)
                                   : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: Theme.fast }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: view.selectedTab === index
                                       ? Theme.foreground
                                       : Theme.muted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.size(12)
                                font.weight: view.selectedTab === index
                                             ? Font.DemiBold : Font.Normal

                                Behavior on color {
                                    ColorAnimation { duration: Theme.fast }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: view.selectedTab = index
                            }
                        }
                    }
                }
            }

            // ---- now playing --------------------------------------------------
            Rectangle {
                visible: !view.showingCalendar && view.hasMedia
                width: parent.width
                height: 108
                radius: 16
                color: Qt.rgba(1, 1, 1, 0.06)

                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    AlbumArt {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 72
                        height: 72
                        cornerRadius: 14
                        source: view.player ? view.player.artUrl : ""
                        fallbackIcon: view.player ? view.player.iconName : "media-optical-audio"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: view.player && view.player.raise()
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 96
                        spacing: 6

                        Marquee {
                            width: parent.width
                            text: view.player ? view.player.title : ""
                            color: Theme.foreground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.size(13)
                            font.weight: Font.DemiBold
                        }

                        Marquee {
                            width: parent.width
                            text: view.player ? view.player.artist : ""
                            color: Theme.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.size(11)
                        }

                        Item {
                            width: parent.width
                            height: 14

                            LevelBar {
                                id: seek
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                fillColor: Theme.accent
                                value: view.player && view.player.length > 0
                                       ? view.player.position / view.player.length
                                       : 0
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: view.player ? view.player.canSeek : false
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: mouse => view.player.setPositionRatio(mouse.x / width)
                            }
                        }

                        // Configurable transport buttons
                        Row {
                            spacing: 2

                            Repeater {
                                model: Cfg.transportButtons

                                delegate: Item {
                                    id: expBtn
                                    required property string modelData

                                    readonly property bool isPlayPause: modelData === "playPause"
                                    width: isPlayPause ? 34 : 28
                                    height: 34

                                    RoundButton {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        width: expBtn.isPlayPause ? 34 : 28
                                        height: width
                                        icon: {
                                            const btn = expBtn.modelData
                                            if (btn === "shuffle") return ["media-playlist-shuffle"]
                                            if (btn === "previous") return ["media-skip-backward"]
                                            if (btn === "playPause") {
                                                return [view.player && view.player.playing
                                                        ? "media-playback-pause"
                                                        : "media-playback-start"]
                                            }
                                            if (btn === "next") return ["media-skip-forward"]
                                            if (btn === "repeat") return ["media-playlist-repeat"]
                                            return []
                                        }
                                        enabled: {
                                            const p = view.player
                                            if (!p) return false
                                            const btn = expBtn.modelData
                                            if (btn === "previous") return p.canGoPrevious
                                            if (btn === "playPause") return p.canPlay || p.canPause
                                            if (btn === "next") return p.canGoNext
                                            return true
                                        }
                                        onClicked: {
                                            const p = view.player
                                            if (!p) return
                                            const btn = expBtn.modelData
                                            if (btn === "shuffle") { p.setShuffle(!p.shuffle) }
                                            else if (btn === "previous") { p.previous() }
                                            else if (btn === "playPause") { p.playPause() }
                                            else if (btn === "next") { p.next() }
                                            else if (btn === "repeat") {
                                                const s = p.loopStatus
                                                p.setLoopStatus(s === "None" || s === ""
                                                                ? "Playlist"
                                                                : s === "Playlist" ? "Track" : "None")
                                            }
                                        }
                                    }

                                    // Active-state dot for shuffle and repeat
                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: Theme.accent
                                        visible: {
                                            const p = view.player
                                            if (!p) return false
                                            const btn = expBtn.modelData
                                            if (btn === "shuffle") return p.shuffle
                                            if (btn === "repeat") {
                                                return p.loopStatus !== "None" && p.loopStatus !== ""
                                            }
                                            return false
                                        }
                                    }
                                }
                            }

                            // Switch between multiple active players
                            RoundButton {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: App.media.count > 1
                                icon: ["go-next-view", "view-refresh"]
                                onClicked: App.media.cycle()
                            }
                        }
                    }
                }
            }

            // ---- lyrics -------------------------------------------------------
            Rectangle {
                visible: !view.showingCalendar && view.hasLyrics
                width: parent.width
                height: visible ? lyrics.implicitHeight + 24 : 0
                radius: 16
                color: Qt.rgba(1, 1, 1, 0.06)

                LyricsView {
                    id: lyrics
                    anchors.fill: parent
                    anchors.margins: 12
                }
            }

            // ---- notifications ------------------------------------------------
            Item {
                width: parent.width
                height: Math.min(168, Math.max(0, App.notifications.count * 58))
                visible: !view.showingCalendar && App.notifications.count > 0

                ListView {
                    id: history
                    anchors.fill: parent
                    clip: true
                    spacing: 6
                    model: App.notifications
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property int index
                        required property var model

                        width: history.width
                        height: 52
                        radius: 12
                        color: Qt.rgba(1, 1, 1, hover.hovered ? 0.1 : 0.05)

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.fast
                            }
                        }

                        HoverHandler {
                            id: hover
                        }

                        Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            IconImage {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 22
                                height: 22
                                names: [model.appIcon, model.appName.toLowerCase(), "dialog-information"]
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 70
                                spacing: 1

                                Text {
                                    width: parent.width
                                    text: model.summary
                                    color: Theme.foreground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.size(11)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                Text {
                                    width: parent.width
                                    text: model.body.replace(/<[^>]*>/g, "")
                                    visible: text.length > 0
                                    color: Theme.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.size(10)
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            RoundButton {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24
                                height: 24
                                opacity: hover.hovered ? 1 : 0
                                icon: ["window-close"]
                                onClicked: App.notifications.close(model.uid)
                            }
                        }
                    }
                }
            }

            // ---- toggles ------------------------------------------------------
            Row {
                visible: !view.showingCalendar
                width: parent.width
                spacing: 8

                QuickToggle {
                    icon: [App.notifications.doNotDisturb ? "notifications-disabled" : "notifications"]
                    label: qsTr("Silence")
                    checked: App.notifications.doNotDisturb
                    onToggled: App.notifications.doNotDisturb = !App.notifications.doNotDisturb
                }

                QuickToggle {
                    icon: ["edit-clear-history", "edit-delete"]
                    label: qsTr("Clear")
                    enabled: App.notifications.count > 0
                    opacity: App.notifications.count > 0 ? 1 : 0.4
                    onToggled: App.notifications.clear()
                }

                QuickToggle {
                    icon: ["audio-volume-muted"]
                    label: qsTr("Mute")
                    onToggled: App.toggleMute()
                }

                QuickToggle {
                    visible: Cfg.modules.lyrics ?? true
                    icon: ["view-media-lyrics", "view-media-track"]
                    label: qsTr("Lyrics")
                    checked: Cfg.lyrics.enabled ?? true
                    onToggled: App.config.setValue("lyrics.enabled", !(Cfg.lyrics.enabled ?? true))
                }

                QuickToggle {
                    icon: ["configure", "settings-configure"]
                    label: qsTr("Settings")
                    onToggled: {
                        App.openSettings()
                        view.collapseRequested()
                    }
                }
            }

            // ---- calendar tab: today's events ---------------------------------
            Column {
                visible: view.showingCalendar
                width: parent.width
                spacing: 6

                readonly property var events: App.calendar ? (App.calendar.todayEvents ?? []) : []

                // "No events" placeholder
                Item {
                    visible: parent.events.length === 0
                    width: parent.width
                    height: 72

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No events today")
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(13)
                    }
                }

                Repeater {
                    model: parent.events

                    delegate: Rectangle {
                        required property var modelData

                        readonly property string evStatus: modelData.status ?? "upcoming"
                        readonly property color statusColor: {
                            if (evStatus === "ongoing")  return Theme.positive
                            if (evStatus === "past")     return Qt.rgba(Theme.muted.r, Theme.muted.g, Theme.muted.b, 0.5)
                            return Theme.accent
                        }

                        width: parent.width
                        height: 52
                        radius: 12
                        color: Qt.rgba(1, 1, 1, evStatus === "ongoing" ? 0.09 : 0.05)
                        opacity: evStatus === "past" ? 0.55 : 1.0

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 10

                            // Coloured status bar
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: 32
                                radius: 2
                                color: statusColor
                            }

                            // Time badge
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 42
                                spacing: 1

                                Text {
                                    width: parent.width
                                    text: modelData.startTime ?? ""
                                    color: statusColor
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.size(10)
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    width: parent.width
                                    visible: (modelData.endTime ?? "").length > 0
                                    text: modelData.endTime ?? ""
                                    color: Theme.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.size(9)
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            // Title and calendar name
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 55 - 10
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: modelData.title ?? ""
                                    color: Theme.foreground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.size(12)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                Row {
                                    spacing: 5

                                    // "ongoing" pill
                                    Rectangle {
                                        visible: evStatus === "ongoing"
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: ongoingLabel.implicitWidth + 8
                                        height: 14
                                        radius: 7
                                        color: Qt.rgba(Theme.positive.r, Theme.positive.g, Theme.positive.b, 0.22)

                                        Text {
                                            id: ongoingLabel
                                            anchors.centerIn: parent
                                            text: qsTr("Now")
                                            color: Theme.positive
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.size(9)
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.calendarName ?? ""
                                        color: Theme.muted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.size(10)
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Scroll indicator — visible only when content overflows.
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 4
        y: scroller.visibleArea.yPosition * scroller.height
        width: 3
        height: Math.max(24, scroller.height * scroller.visibleArea.heightRatio)
        radius: 1.5
        color: Qt.rgba(1, 1, 1, 0.4)
        visible: scroller.contentHeight > scroller.height + 4
        opacity: scroller.moving || scroller.flicking ? 1.0 : 0.5

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.fast
            }
        }
    }
}
