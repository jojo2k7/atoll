// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A single notification, morphed into. The island mirrors what the real
 * notification daemon received, so this is a second, glanceable surface rather
 * than a replacement popup.
 */
Item {
    id: view

    property var notification: ({})

    readonly property bool critical: (notification.urgency ?? 1) >= 2
    readonly property string image: notification.image ?? ""
    readonly property bool hasProgress: (notification.progress ?? -1) >= 0

    signal dismissRequested()

    implicitHeight: 74
    implicitWidth: Math.min(Cfg.maxWidth,
                            Math.max(320, layout.implicitWidth + Math.max(34, Theme.edgeInset * 2)))

    Row {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Math.max(15, Theme.edgeInset)
        anchors.rightMargin: Math.max(15, Theme.edgeInset)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Item {
            width: 40
            height: 40
            anchors.verticalCenter: parent.verticalCenter

            AlbumArt {
                anchors.fill: parent
                visible: view.image.length > 0
                source: view.image
                cornerRadius: 10
            }

            Rectangle {
                anchors.fill: parent
                visible: view.image.length === 0
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.08)

                IconImage {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    names: [view.notification.appIcon ?? "",
                            view.notification.desktopEntry ?? "",
                            "dialog-information"]
                }
            }

            Rectangle {
                visible: view.critical
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: -2
                width: 10
                height: 10
                radius: 5
                color: Theme.critical
                border.width: 2
                border.color: Theme.background
            }
        }

        Column {
            width: Math.min(320, Math.max(180, summary.implicitWidth, body.implicitWidth))
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                id: appLabel
                width: parent.width
                text: view.notification.appName ?? ""
                visible: text.length > 0
                color: view.critical ? Theme.critical : Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                id: summary
                width: parent.width
                text: view.notification.summary ?? ""
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(13)
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                id: body
                width: parent.width
                text: (view.notification.body ?? "").replace(/<[^>]*>/g, "")
                visible: text.length > 0 && !view.hasProgress
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(11)
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: Cfg.notifications.maxBodyLines ?? 2
            }

            LevelBar {
                visible: view.hasProgress
                width: parent.width
                height: 5
                value: (view.notification.progress ?? 0) / 100
                fillColor: Theme.accent
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => {
            if (mouse.button === Qt.MiddleButton) {
                App.notifications.close(view.notification.uid ?? 0)
            } else {
                // Hand the app its default action and bring it forward, the
                // way a Plasma popup does.
                App.notifications.open(view.notification.uid ?? 0)
            }
            view.dismissRequested()
        }
    }
}
