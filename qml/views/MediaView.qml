// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Now playing, in the compact form: art, title, a spectrum, and transport
 * controls that fade in when the pointer is over the island.
 *
 * The second line is the one that changes: normally the artist (and album, if
 * asked for), but the line being sung right now whenever synced lyrics exist
 * for the track.
 */
Item {
    id: view

    property bool interactive: false
    readonly property var player: App.media.active

    readonly property string lyric: Cfg.lyricsInIsland ? App.lyrics.currentLine : ""
    readonly property string subtitle: {
        if (!player) {
            return ""
        }
        if (lyric.length > 0) {
            return lyric
        }
        if ((Cfg.media.showAlbum ?? true) && player.album.length > 0 && player.artist.length > 0) {
            return player.artist + " - " + player.album
        }
        return player.artist
    }

    // Transport buttons this player actually supports: the classic three
    // always, shuffle and repeat only when the player exposes those MPRIS
    // properties at all.
    readonly property var transportModel: {
        const p = view.player
        const b = ["previous", "playPause", "next"]
        if (p && p.canShuffle) b.unshift("shuffle")
        if (p && p.canLoop) b.push("repeat")
        return b
    }

    // Sum of widths of the transport buttons.
    readonly property int transportWidth: {
        let w = 0
        for (const b of view.transportModel) {
            w += (b === "playPause") ? 28 : 26
        }
        return Math.max(26, w)
    }

    implicitHeight: 52
    implicitWidth: Math.min(Cfg.maxWidth,
                            Math.max(340, layout.implicitWidth + Math.max(32, Theme.edgeInset * 2)))

    Row {
        id: layout
        anchors.centerIn: parent
        spacing: 12

        AlbumArt {
            id: cover
            anchors.verticalCenter: parent.verticalCenter
            width: 36
            height: 36
            visible: Cfg.media.showArt ?? true
            source: view.player ? view.player.artUrl : ""
            fallbackIcon: view.player ? view.player.iconName : "media-optical-audio"
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: 178
            spacing: 1

            Marquee {
                width: parent.width
                text: view.player && view.player.title.length > 0
                      ? view.player.title
                      : (view.player ? view.player.identity : "")
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
            }

            Marquee {
                id: second
                width: parent.width
                visible: text.length > 0
                text: view.subtitle
                color: view.lyric.length > 0 ? Theme.accent : Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)

                transform: Translate { id: slideT; y: 0 }

                onTextChanged: if (view.lyric.length > 0) {
                    lineIn.restart()
                    lineSlide.restart()
                }

                NumberAnimation {
                    id: lineIn
                    target: second
                    property: "opacity"
                    from: 0.25
                    to: 1
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }

                NumberAnimation {
                    id: lineSlide
                    target: slideT
                    property: "y"
                    from: 5
                    to: 0
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }

        // The spectrum yields its space to the controls on hover.
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: view.interactive ? view.transportWidth : 26
            height: 28

            Behavior on width {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }

            Spectrum {
                id: spectrum
                anchors.centerIn: parent
                height: parent.height
                barWidth: 2
                limit: 5
                opacity: view.interactive ? 0 : 1
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.fast
                    }
                }
            }

            Row {
                anchors.centerIn: parent
                spacing: 0
                opacity: view.interactive ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.fast
                    }
                }

                Repeater {
                    model: view.transportModel

                    delegate: Item {
                        id: compactBtn
                        required property string modelData

                        readonly property bool isPlayPause: modelData === "playPause"
                        width: isPlayPause ? 28 : 26
                        height: 28

                        RoundButton {
                            anchors.centerIn: parent
                            width: compactBtn.isPlayPause ? 28 : 26
                            height: width
                            opacity: {
                                const p = view.player
                                if (!p) return 0.65
                                const btn = compactBtn.modelData
                                if (btn === "shuffle") return p.shuffle ? 1.0 : 0.55
                                if (btn === "repeat") return (p.loopStatus && p.loopStatus !== "None") ? 1.0 : 0.55
                                return 1.0
                            }
                            icon: {
                                const btn = compactBtn.modelData
                                if (btn === "shuffle") return ["media-playlist-shuffle"]
                                if (btn === "previous") return ["media-skip-backward"]
                                if (btn === "playPause") {
                                    return [view.player && view.player.playing
                                            ? "media-playback-pause" : "media-playback-start"]
                                }
                                if (btn === "next") return ["media-skip-forward"]
                                if (btn === "repeat") return ["media-playlist-repeat"]
                                return []
                            }
                            enabled: {
                                const p = view.player
                                if (!p) return false
                                const btn = compactBtn.modelData
                                if (btn === "previous") return p.canGoPrevious
                                if (btn === "playPause") return p.canPlay || p.canPause
                                if (btn === "next") return p.canGoNext
                                return true
                            }
                            onClicked: {
                                const p = view.player
                                if (!p) return
                                const btn = compactBtn.modelData
                                if (btn === "shuffle") { p.setShuffle(!p.shuffle) }
                                else if (btn === "previous") { p.previous() }
                                else if (btn === "playPause") { p.playPause() }
                                else if (btn === "next") { p.next() }
                                else if (btn === "repeat") {
                                    const s = p.loopStatus
                                    p.setLoopStatus(s === "None" || s === ""
                                                    ? "Playlist" : s === "Playlist" ? "Track" : "None")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
