// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import Atoll

/**
 * The settings window.
 *
 * It runs in its own process (`atoll --settings`) and edits the same JSON file
 * the island watches, so every change lands on the island as it is made -
 * there is no apply button, and nothing to keep in sync.
 */
Window {
    id: root

    property int current: 0

    readonly property var pages: [
        { title: qsTr("Placement"), icon: ["preferences-desktop-display", "video-display"] },
        { title: qsTr("Appearance"), icon: ["preferences-desktop-theme", "preferences-desktop-color"] },
        { title: qsTr("Content"), icon: ["view-list-details", "preferences-desktop"] },
        { title: qsTr("Media"), icon: ["multimedia-player", "audio-x-generic"] },
        { title: qsTr("Notifications"), icon: ["preferences-desktop-notification", "notifications"] },
        { title: qsTr("Behaviour"), icon: ["input-mouse", "preferences-desktop-mouse"] },
        { title: qsTr("Sharing"), icon: ["emblem-shared", "document-send"] },
        { title: qsTr("Calendar"), icon: ["view-calendar", "office-calendar", "x-office-calendar"] },
        { title: qsTr("Assistant"), icon: ["preferences-desktop-ai", "tools-wizard", "help-hint"] },
        { title: qsTr("About"), icon: ["help-about", "dialog-information"] }
    ]

    /** Page names the island can ask to land on. */
    readonly property var pageKeys: ["placement", "appearance", "content", "media",
                                     "notifications", "behaviour", "sharing", "calendar", "ai", "about"]

    Component.onCompleted: {
        // The island opens this window on a specific page when it is offering
        // to fix something the user has not set up yet.
        const wanted = pageKeys.indexOf(App.settingsPage)
        if (wanted >= 0) {
            current = wanted
        }
    }

    width: 940
    height: 700
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    color: Skin.window
    title: qsTr("Atoll Settings")

    Connections {
        target: App.ipc

        function onRaiseRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    // The window follows the same album-art accent the island does, so the two
    // read as one thing while you are configuring it.
    Binding {
        target: Theme
        property: "dynamicAccent"
        value: App.media.active && App.media.active.artUrl.length > 0 && App.media.active.accent.a > 0
               ? App.media.active.accent
               : Theme.accentFallback
        restoreMode: Binding.RestoreNone
    }

    Shortcut {
        sequences: [StandardKey.Close, StandardKey.Quit, "Esc"]
        onActivated: root.close()
    }

    // ---- sidebar ---------------------------------------------------------
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 224
        color: Skin.sidebar

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Skin.line
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            anchors.topMargin: 22
            spacing: 4

            Row {
                x: 8
                spacing: 10
                bottomPadding: 16

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    height: 14
                    radius: 7
                    color: Skin.accent
                }

                Column {
                    spacing: 0

                    Text {
                        text: "Atoll"
                        color: Skin.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: qsTr("version %1").arg(App.version)
                        color: Skin.muted
                        font.pixelSize: 10
                    }
                }
            }

            Repeater {
                model: root.pages

                delegate: Rectangle {
                    id: entry

                    required property int index
                    required property var modelData

                    readonly property bool active: index === root.current

                    width: sidebar.width - 28
                    height: 38
                    radius: 10
                    color: active ? Skin.cardHover : (hover.hovered ? "#131319" : "transparent")

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: entry.active ? 18 : 0
                        radius: 2
                        color: Skin.accent

                        Behavior on height {
                            NumberAnimation {
                                duration: 140
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        IconImage {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            names: entry.modelData.icon
                            opacity: entry.active ? 1 : 0.7
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.title
                            color: entry.active ? Skin.text : Skin.muted
                            font.pixelSize: 13
                            font.weight: entry.active ? Font.DemiBold : Font.Normal
                        }
                    }

                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: root.current = entry.index
                    }
                }
            }
        }

        // ---- live island preview ------------------------------------------
        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 8

            Text {
                text: qsTr("NOW PLAYING")
                color: Skin.muted
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
                visible: App.media.active !== null
            }

            Rectangle {
                width: parent.width
                height: 56
                radius: 12
                color: Skin.card
                border.width: 1
                border.color: Skin.line
                visible: App.media.active !== null

                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    AlbumArt {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36
                        height: 36
                        source: App.media.active ? App.media.active.artUrl : ""
                        fallbackIcon: App.media.active ? App.media.active.iconName : "media-optical-audio"
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 56
                        spacing: 2

                        Text {
                            width: parent.width
                            text: App.media.active ? App.media.active.title : ""
                            color: Skin.text
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: App.lyrics.currentLine.length > 0
                                  ? App.lyrics.currentLine
                                  : (App.media.active ? App.media.active.artist : "")
                            color: App.lyrics.currentLine.length > 0 ? Skin.accent : Skin.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // ---- pages -----------------------------------------------------------
    Item {
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        PlacementPage {
            anchors.fill: parent
            visible: root.current === 0
        }

        AppearancePage {
            anchors.fill: parent
            visible: root.current === 1
        }

        ContentPage {
            anchors.fill: parent
            visible: root.current === 2
        }

        MediaPage {
            anchors.fill: parent
            visible: root.current === 3
        }

        NotificationsPage {
            anchors.fill: parent
            visible: root.current === 4
        }

        BehaviorPage {
            anchors.fill: parent
            visible: root.current === 5
        }

        SharingPage {
            anchors.fill: parent
            visible: root.current === 6
        }

        CalendarPage {
            anchors.fill: parent
            visible: root.current === 7
        }

        AiPage {
            anchors.fill: parent
            visible: root.current === 8
        }

        AboutPage {
            anchors.fill: parent
            visible: root.current === 9
        }
    }
}
