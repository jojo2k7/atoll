// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * One frame of a manifest sheet, placed by its anchor at (`px`, `py`) and
 * drawn 1:1 with nearest-neighbour sampling.
 *
 * `frame` is an index or a frame name; `animation` plays a named animation off
 * the shared clock, offset by `phase` seconds and restarted by `restart()`.
 * `variant` picks the row of a variants sheet (the characters' look).
 */
Item {
    id: sprite

    property string sheet: ""
    property var frame: undefined
    property string animation: ""
    property int variant: 0
    property real phase: 0
    property real startedAt: 0
    /** Playback speed of `animation`, 1 = the manifest's fps. */
    property real rate: 1
    property bool mirrored: false
    property real px: 0
    property real py: 0

    readonly property var s: OfficeArt.ready ? OfficeArt.sheet(sheet) : undefined
    readonly property int frameWidth: s ? s.frameWidth : 0
    readonly property int frameHeight: s ? s.frameHeight : 0
    readonly property int anchorX: s ? s.anchor[0] : 0
    readonly property int anchorY: s ? s.anchor[1] : 0

    readonly property int frameIdx: {
        if (!s) {
            return 0
        }
        if (animation.length > 0 && s.animations && s.animations[animation]) {
            return OfficeArt.animFrame(s, animation, (OfficeArt.clock - startedAt) * rate + phase)
        }
        return OfficeArt.frameIndex(s, frame)
    }

    function restart() {
        startedAt = OfficeArt.clock
        phase = 0
    }

    x: Math.round(px) - anchorX
    y: Math.round(py) - anchorY
    width: frameWidth
    height: frameHeight
    clip: true

    Image {
        id: sheetImage
        source: sprite.s ? OfficeArt.base + sprite.s.file : ""
        smooth: false
        mipmap: false
        cache: true
        x: sprite.s ? (sprite.s.variants ? -sprite.frameIdx * sprite.frameWidth
                                         : -(sprite.frameIdx % sprite.s.columns) * sprite.frameWidth) : 0
        y: sprite.s ? (sprite.s.variants ? -sprite.variant * sprite.frameHeight
                                         : -Math.floor(sprite.frameIdx / sprite.s.columns) * sprite.frameHeight) : 0
        transform: Scale {
            xScale: sprite.mirrored ? -1 : 1
            origin.x: -sheetImage.x + sprite.frameWidth / 2
        }
    }
}
