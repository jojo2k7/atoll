// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import Atoll

/** A scrollable page of setting groups, under its icon and heading. */
Flickable {
    id: page

    default property alias content: column.data
    property string title: ""
    property string description: ""
    property string glyph: ""
    property color tint: Skin.control

    contentWidth: width
    contentHeight: column.implicitHeight + Skin.pagePadding * 2
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    ScrollBar.vertical: ScrollBar {
        policy: page.contentHeight > page.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }

    Column {
        id: column
        x: Math.max(Skin.pagePadding, (page.width - 680) / 2)
        y: Skin.pagePadding - 8
        width: Math.min(680, page.width - Skin.pagePadding * 2)
        spacing: 22

        Row {
            width: parent.width
            spacing: 14
            visible: page.title.length > 0

            IconTile {
                anchors.top: parent.top
                size: 44
                glyph: page.glyph
                tint: page.tint
                visible: page.glyph.length > 0
            }

            Column {
                width: parent.width - (page.glyph.length > 0 ? 58 : 0)
                spacing: 3

                Text {
                    text: page.title
                    color: Skin.text
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }

                Text {
                    width: parent.width
                    text: page.description
                    visible: text.length > 0
                    color: Skin.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
