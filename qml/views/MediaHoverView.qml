// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

// Three quiet rows, matching the supplied macOS notch reference.
Item {
    id: view
    readonly property var player: App.media.active
    readonly property real position: player ? player.position : 0
    readonly property real duration: player ? player.length : 0
    readonly property color accent: Theme.accent
    readonly property var buttons: Cfg.get("media.hoverButtons", ["shuffle", "previous", "playPause", "next", "output"])
        .filter(b => ["shuffle", "previous", "playPause", "next", "repeat", "output"].indexOf(b) >= 0).slice(0, 7)
    implicitWidth: Math.min(Cfg.maxWidth, Math.max(280, Cfg.get("media.hoverWidth", 360)))
    implicitHeight: Math.max(Math.round(implicitWidth * .5), content.implicitHeight + 40)
    function timestamp(microseconds) {
        const seconds = Math.max(0, Math.floor(microseconds / 1000000))
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toString().padStart(2, "0")
    }
    Column {
        id: content
        x: 20; y: 24; width: parent.width - 40
        spacing: 10
        Row {
            width: parent.width; spacing: 10
            AlbumArt {
                width: 48; height: 48; cornerRadius: 10
                visible: Cfg.media.showArt ?? true
                source: view.player ? view.player.artUrl : ""
                fallbackIcon: view.player ? view.player.iconName : "audio-x-generic"
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - ((Cfg.media.showArt ?? true) ? 58 : 0) - 24
                spacing: 3
                Marquee {
                    width: parent.width
                    text: view.player ? (view.player.title || view.player.identity) : "Not playing"
                    font.pixelSize: 12; font.weight: Font.DemiBold; color: "#f5f5f7"
                }
                Marquee { width: parent.width; text: view.player ? view.player.artist : ""; font.pixelSize: 10; color: view.accent }
            }
            Spectrum { anchors.verticalCenter: parent.verticalCenter; height: 16; limit: 4; barWidth: 2; spacing: 2 }
        }
        Row {
            width: parent.width; height: 14; spacing: 10
            Text { width: 32; anchors.verticalCenter: parent.verticalCenter; text: view.timestamp(view.position); color: view.accent; font.pixelSize: 10 }
            Item {
                width: parent.width - 88; height: 14
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 5; radius: 3; color: "#2a2a2c"
                    Rectangle { width: parent.width * (seek.pressed ? seek.ratio : (view.duration > 0 ? Math.min(1, view.position / view.duration) : 0)); height: parent.height; radius: 3; color: "#f5f5f7" }
                }
                MouseArea {
                    id: seek
                    anchors.fill: parent; anchors.topMargin: -5; anchors.bottomMargin: -5
                    property real ratio: 0
                    enabled: view.player !== null && view.player.canSeek && view.duration > 0
                    cursorShape: Qt.PointingHandCursor
                    onPressed: mouse => { ratio = Math.max(0, Math.min(1, mouse.x / width)) }
                    onPositionChanged: mouse => { if (pressed) ratio = Math.max(0, Math.min(1, mouse.x / width)) }
                    onReleased: view.player.setPositionRatio(ratio)
                    Accessible.role: Accessible.Slider
                    Accessible.name: "Playback position"
                }
            }
            Text { width: 36; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter; text: "−" + view.timestamp(view.duration - view.position); color: view.accent; font.pixelSize: 10 }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Repeater {
                model: view.buttons
                delegate: TransportButton {
                    required property string modelData
                    width: Math.min(42, (view.width - 60 - 10 * (view.buttons.length - 1)) / Math.max(1, view.buttons.length)); height: 38
                    glyphSize: modelData === "playPause" ? 26 : modelData === "output" || modelData === "shuffle" ? 21 : 24
                    action: modelData === "playPause" ? (view.player && view.player.playing ? "pause" : "play") : modelData
                    selected: view.player !== null && (modelData === "shuffle" ? view.player.shuffle : modelData === "repeat" && view.player.loopStatus !== "None")
                    label: ({shuffle: "Shuffle", previous: "Previous track", playPause: view.player && view.player.playing ? "Pause" : "Play", next: "Next track", output: qsTr("Audio output"), repeat: "Repeat"})[modelData] || modelData
                    enabled: modelData === "output" || view.player !== null && (modelData === "previous" ? view.player.canGoPrevious : modelData === "next" ? view.player.canGoNext : modelData === "playPause" ? view.player.canPlay || view.player.canPause : true)
                    onClicked: {
                        if (modelData === "output") { outputs.expanded = !outputs.expanded; return }
                        const p = view.player
                        if (!p) return
                        if (modelData === "shuffle") p.shuffle = !p.shuffle
                        else if (modelData === "previous") p.previous()
                        else if (modelData === "playPause") p.playPause()
                        else if (modelData === "next") p.next()
                        else if (modelData === "repeat") p.loopStatus = p.loopStatus === "None" ? "Playlist" : "None"
                    }
                }
            }
        }
        AudioOutputPicker {
            id: outputs
            width: parent.width
            showHeader: false
        }
    }
}
