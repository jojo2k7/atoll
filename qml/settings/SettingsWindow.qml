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

    property string current: "placement"
    property string query: ""

    /**
     * Every page, in sidebar order. `group` starts a new block in the sidebar;
     * `words` are what the search field matches besides the title.
     */
    readonly property var pages: [
        { key: "placement", file: "PlacementPage.qml", title: qsTr("Placement"), glyph: "placement", tint: "#0a84ff", group: "",
          words: "monitor screen position corner notch pill size width height margin layer" },
        { key: "appearance", file: "AppearancePage.qml", title: qsTr("Appearance"), glyph: "appearance", tint: "#bf5af2", group: "",
          words: "colour color accent background font border shadow animation spring gooey" },
        { key: "content", file: "ContentPage.qml", title: qsTr("Content"), glyph: "content", tint: "#30b158", group: "",
          words: "clock date idle resting modules battery bluetooth privacy lock screen osd volume" },

        { key: "media", file: "MediaPage.qml", title: qsTr("Media"), glyph: "media", tint: "#ff375f", group: qsTr("Activities"),
          words: "music player spotify lyrics visualizer cava album cover" },
        { key: "notifications", file: "NotificationsPage.qml", title: qsTr("Notifications"), glyph: "notifications", tint: "#ff453a", group: qsTr("Activities"),
          words: "do not disturb dnd timeout apps actions" },
        { key: "calendar", file: "CalendarPage.qml", title: qsTr("Calendar"), glyph: "calendar", tint: "#ff9f0a", group: qsTr("Activities"),
          words: "ics google icloud events meeting" },
        { key: "focus", file: "FocusPage.qml", title: qsTr("Focus"), glyph: "focus", tint: "#5e5ce6", group: qsTr("Activities"),
          words: "pomodoro timer break session sound" },

        { key: "controls", file: "ControlsPage.qml", title: qsTr("Controls"), glyph: "controls", tint: "#8e8e93", group: qsTr("Interaction"),
          words: "click middle right scroll hover hold long press shortcut hotkey keyboard behaviour behavior mouse" },
        { key: "sharing", file: "SharingPage.qml", title: qsTr("Sharing"), glyph: "sharing", tint: "#32ade6", group: qsTr("Interaction"),
          words: "localsend airdrop files drop send receive" },

        { key: "ai", file: "AiPage.qml", title: qsTr("Assistant"), glyph: "assistant", tint: "#2fb7c9", group: qsTr("Claude"),
          words: "ai claude gemini openrouter model api key permissions glow screenshot" },
        { key: "office", file: "OfficePage.qml", title: qsTr("Claude Office"), glyph: "office", tint: "#d97757", group: qsTr("Claude"),
          words: "agents claude code sessions hooks questions" },

        { key: "about", file: "AboutPage.qml", title: qsTr("About"), glyph: "about", tint: "#8e8e93", group: " ",
          words: "version license credits" }
    ]

    readonly property var currentPage: pages.find(p => p.key === current) ?? pages[0]

    readonly property var visiblePages: {
        const q = query.trim().toLowerCase()
        if (q.length === 0) {
            return pages
        }
        return pages.filter(p => (p.title + " " + p.words).toLowerCase().includes(q))
    }

    Component.onCompleted: {
        // The island opens this window on a specific page when it is offering
        // to fix something the user has not set up yet.
        const wanted = App.settingsPage === "behaviour" ? "controls" : App.settingsPage
        if (pages.some(p => p.key === wanted)) {
            current = wanted
        }
    }

    width: 960
    height: 720
    minimumWidth: 760
    minimumHeight: 540
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

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: search.forceActiveFocus()
    }

    // ---- sidebar ---------------------------------------------------------
    Rectangle {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 236
        color: Skin.sidebar

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Skin.line
        }

        Row {
            id: brand
            x: 20
            y: 22
            spacing: 10

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 17
                sourceSize: Qt.size(68, 34)
                source: "qrc:/qt/qml/Atoll/data/icons/atoll-mark.svg"
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: "Atoll"
                    color: Skin.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                Text {
                    text: qsTr("Version %1").arg(App.version)
                    color: Skin.faint
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            id: searchBox
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: brand.bottom
            anchors.margins: 14
            anchors.topMargin: 18
            height: 28
            radius: Skin.radius
            color: Skin.field
            border.width: search.activeFocus ? 2 : 0
            border.color: Qt.alpha(Skin.accent, 0.8)

            Image {
                id: searchIcon
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                sourceSize: Qt.size(28, 28)
                source: Skin.icon("search")
            }

            TextInput {
                id: search
                anchors.left: searchIcon.right
                anchors.leftMargin: 6
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: Skin.text
                selectionColor: Skin.accent
                font.pixelSize: 12
                clip: true
                onTextChanged: root.query = text
                Keys.onEscapePressed: {
                    text = ""
                    focus = false
                }
                Keys.onReturnPressed: if (root.visiblePages.length > 0) {
                    root.current = root.visiblePages[0].key
                }
            }

            Text {
                anchors.left: search.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Search")
                color: Skin.faint
                font.pixelSize: 12
                visible: search.text.length === 0
            }
        }

        ListView {
            id: nav
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: searchBox.bottom
            anchors.bottom: nowPlaying.top
            anchors.topMargin: 10
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.visiblePages
            spacing: 1

            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                required property string section

                width: ListView.view.width
                height: section.length > 0 && root.query.length === 0 ? 30 : 0
                visible: height > 0

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    text: section.trim()
                    color: Skin.faint
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }

            delegate: Rectangle {
                id: entry

                required property var modelData

                readonly property bool active: modelData.key === root.current

                width: ListView.view.width
                height: 32
                radius: 7
                color: active ? Skin.selection : (hover.hovered ? "#09ffffff" : "transparent")

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    IconTile {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 22
                        glyph: entry.modelData.glyph
                        tint: entry.modelData.tint
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.title
                        color: entry.active ? Skin.text : "#d4d4da"
                        font.pixelSize: 13
                        font.weight: entry.active ? Font.DemiBold : Font.Normal
                    }
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: root.current = entry.modelData.key
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 16
                text: qsTr("No matches")
                color: Skin.faint
                font.pixelSize: 12
                visible: root.visiblePages.length === 0
            }
        }

        // ---- what is playing, as a reminder of where the accent comes from
        Item {
            id: nowPlaying
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: App.media.active !== null ? 56 : 0
            visible: height > 0

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                height: 1
                color: Skin.line
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                AlbumArt {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    source: App.media.active ? App.media.active.artUrl : ""
                    fallbackIcon: App.media.active ? App.media.active.iconName : "media-optical-audio"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: sidebar.width - 80

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

    // ---- pages -----------------------------------------------------------
    Loader {
        id: pageLoader
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        source: Qt.resolvedUrl(root.currentPage.file)

        onLoaded: {
            item.glyph = root.currentPage.glyph
            item.tint = root.currentPage.tint
            item.opacity = 0
            fadeIn.restart()
        }

        NumberAnimation {
            id: fadeIn
            target: pageLoader.item
            property: "opacity"
            to: 1
            duration: 140
            easing.type: Easing.OutCubic
        }
    }
}
