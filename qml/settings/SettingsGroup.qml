// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A titled run of setting rows. There is no card around it: the heading and
 * the hairlines between rows carry the structure, so a page reads as one
 * sheet instead of a stack of boxes.
 */
Column {
    id: group

    property string title: ""
    property string footer: ""
    default property alias rows: body.data

    width: parent ? parent.width : 0
    spacing: 2
    topPadding: 10

    Text {
        text: group.title
        visible: text.length > 0
        color: Skin.text
        font.pixelSize: 13
        font.weight: Font.DemiBold
        bottomPadding: 4
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Skin.line
        visible: group.title.length > 0
    }

    Column {
        id: body
        width: parent.width
    }

    Text {
        width: parent.width
        text: group.footer
        visible: text.length > 0
        color: Skin.faint
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        topPadding: 6
    }
}
