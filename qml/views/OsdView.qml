// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Volume, brightness, keyboard layout - whatever Plasma just announced.
 * Progress-less events (a power profile switch, Wi-Fi toggling) collapse to a
 * plain icon and label instead of showing an empty bar.
 */
Item {
    id: view

    readonly property var osd: App.osd
    readonly property bool withBar: osd.hasProgress
    readonly property real ratio: Math.min(1, osd.percent / Math.max(100, osd.maxPercent))
    // Plasma reports over-amplification by raising maxPercent above 100.
    readonly property bool boosted: osd.percent > 100

    implicitHeight: 46
    implicitWidth: Math.min(Cfg.maxWidth, row.implicitWidth + Math.max(36, Theme.edgeInset * 2))

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12

        IconImage {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            names: [view.osd.icon, "dialog-information"]
        }

        Item {
            visible: view.withBar
            anchors.verticalCenter: parent.verticalCenter
            width: 168
            height: 20

            LevelBar {
                id: level
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 6
                value: view.ratio
                fillColor: view.osd.percent <= 0
                           ? Theme.muted
                           : (view.boosted ? Theme.critical : Theme.foreground)
            }
        }

        Text {
            visible: view.withBar
            anchors.verticalCenter: parent.verticalCenter
            text: view.osd.percent + "%"
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(12)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
            width: 38
        }

        Text {
            visible: !view.withBar
            anchors.verticalCenter: parent.verticalCenter
            text: view.osd.label.length > 0
                  ? view.osd.label
                  : view.osd.icon.split("-").map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(" ")
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(13)
            font.weight: Font.Medium
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
