// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A bundled glyph on a coloured squircle, the way each page is recognised. */
Rectangle {
    id: tile

    property string glyph: ""
    property color tint: Skin.control
    property int size: 20

    width: size
    height: size
    radius: Math.round(size * 0.27)
    gradient: Gradient {
        GradientStop { position: 0; color: Qt.lighter(tile.tint, 1.12) }
        GradientStop { position: 1; color: tile.tint }
    }

    Image {
        anchors.centerIn: parent
        width: Math.round(tile.size * 0.66)
        height: width
        sourceSize.width: width * 2
        sourceSize.height: height * 2
        source: tile.glyph.length > 0 ? Skin.icon(tile.glyph) : ""
        smooth: true
    }
}
