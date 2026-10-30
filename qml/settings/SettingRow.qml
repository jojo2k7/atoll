// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * One setting: a label and its explanation on the left, whatever edits it on
 * the right. Controls that need room - a list, a text field - set `wide` and
 * get the full width underneath the label instead.
 */
Item {
    id: row

    property string label: ""
    property string description: ""
    property bool wide: false
    property bool last: false
    default property alias controlData: slot.data

    width: parent ? parent.width : 0
    implicitHeight: row.wide
                    ? labels.implicitHeight + slot.childrenRect.height + 30
                    : Math.max(Skin.rowHeight, labels.implicitHeight + 22, slot.childrenRect.height + 16)

    Column {
        id: labels
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: row.wide ? 12 : Math.max(11, (row.height - labels.implicitHeight) / 2)
        width: row.wide ? row.width : row.width - slot.childrenRect.width - 32
        spacing: 3

        Text {
            text: row.label
            color: Skin.text
            font.pixelSize: 13
            elide: Text.ElideRight
            width: parent.width
        }

        Text {
            width: parent.width
            text: row.description
            visible: text.length > 0
            color: Skin.muted
            font.pixelSize: 11
            lineHeight: 1.1
            wrapMode: Text.WordWrap
        }
    }

    Item {
        id: slot
        anchors.right: row.wide ? undefined : parent.right
        anchors.left: row.wide ? parent.left : undefined
        anchors.top: row.wide ? labels.bottom : undefined
        anchors.topMargin: 10
        anchors.verticalCenter: row.wide ? undefined : parent.verticalCenter
        width: row.wide ? row.width : childrenRect.width
        height: childrenRect.height
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Skin.line
        visible: !row.last
    }
}
