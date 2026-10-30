// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A 9-slice of one frame of a manifest sheet (its `slice` borders): corners
 * 1:1, edges and centre stretched nearest-neighbour. Width and height are
 * rounded so the pieces never land between pixels.
 */
Item {
    id: nine

    property string sheet: ""
    property var frame: undefined

    readonly property var s: OfficeArt.ready ? OfficeArt.sheet(sheet) : undefined
    readonly property int fi: s ? OfficeArt.frameIndex(s, frame) : 0
    readonly property int fx: s ? (fi % s.columns) * s.frameWidth : 0
    readonly property int fy: s ? Math.floor(fi / s.columns) * s.frameHeight : 0
    readonly property int fw: s ? s.frameWidth : 0
    readonly property int fh: s ? s.frameHeight : 0
    readonly property var cut: s && s.slice ? s.slice : [0, 0, 0, 0]
    readonly property int w: Math.round(width)
    readonly property int h: Math.round(height)
    readonly property string src: s ? OfficeArt.base + s.file : ""

    // [source x, source width, dest x, dest width] per column, same for rows.
    readonly property var cols: [[0, cut[0], 0, cut[0]],
                                 [cut[0], fw - cut[0] - cut[2], cut[0], w - cut[0] - cut[2]],
                                 [fw - cut[2], cut[2], w - cut[2], cut[2]]]
    readonly property var rows: [[0, cut[1], 0, cut[1]],
                                 [cut[1], fh - cut[1] - cut[3], cut[1], h - cut[1] - cut[3]],
                                 [fh - cut[3], cut[3], h - cut[3], cut[3]]]

    Repeater {
        model: nine.s ? 9 : 0

        Image {
            required property int index
            readonly property var c: nine.cols[index % 3]
            readonly property var r: nine.rows[Math.floor(index / 3)]
            visible: c[1] > 0 && r[1] > 0 && c[3] > 0 && r[3] > 0
            source: visible ? nine.src : ""
            sourceClipRect: Qt.rect(nine.fx + c[0], nine.fy + r[0], c[1], r[1])
            x: c[2]
            y: r[2]
            width: c[3]
            height: r[3]
            fillMode: Image.Stretch
            smooth: false
        }
    }
}
