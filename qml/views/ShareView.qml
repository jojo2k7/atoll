// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Sharing, in every state it can be in: a target while files are being dragged
 * over the island, a list of nearby devices once they have been dropped, the
 * transfer itself, and the same again in reverse for files arriving.
 *
 * The protocol underneath is LocalSend. AirDrop is not an option on Linux -
 * it rides on Apple's own AWDL link layer, and the open re-implementations of
 * it need a wifi card that can be taken off the network and driven in monitor
 * mode - whereas LocalSend is documented, cross-platform and ordinary wifi.
 */
Item {
    id: view

    /** True while a drag is hovering the island, before anything is dropped. */
    property bool dragging: false

    readonly property var share: App.share
    readonly property string phase: dragging ? "drop" : share.state

    implicitWidth: Math.min(Cfg.maxWidth,
                            Math.max(300, (loader.item ? loader.item.implicitWidth : 0)
                                          + Math.max(44, Theme.edgeInset * 2)))
    implicitHeight: (loader.item ? loader.item.implicitHeight : 24) + 30

    function humanSize(bytes) {
        if (bytes >= 1073741824) {
            return (bytes / 1073741824).toFixed(1) + " GB"
        }
        if (bytes >= 1048576) {
            return (bytes / 1048576).toFixed(1) + " MB"
        }
        if (bytes >= 1024) {
            return Math.round(bytes / 1024) + " kB"
        }
        return bytes + " B"
    }

    function summary() {
        const count = share.fileCount
        const files = count === 1 ? share.files[0].name : qsTr("%n files", "", count)
        return files + " · " + humanSize(share.totalBytes)
    }

    function deviceIcon(type) {
        switch (type) {
        case "mobile":
            return ["smartphone", "phone", "computer"]
        case "web":
            return ["internet-web-browser", "computer"]
        case "server":
        case "headless":
            return ["network-server", "computer"]
        default:
            return ["computer", "computer-laptop"]
        }
    }

    Loader {
        id: loader
        anchors.centerIn: parent
        sourceComponent: {
            switch (view.phase) {
            case "drop":
                return dropTarget
            case "staged":
                return picker
            case "incoming":
                return request
            case "sending":
            case "receiving":
                return transfer
            default:
                return result
            }
        }
    }

    // ---- a drag is hovering ------------------------------------------------
    Component {
        id: dropTarget

        Column {
            spacing: 7

            IconImage {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 26
                height: 26
                names: ["document-send", "emblem-shared", "folder-download"]
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Drop to share")
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
            }
        }
    }

    // ---- files are staged, waiting for a device ----------------------------
    Component {
        id: picker

        Column {
            spacing: 9

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: view.summary()
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                maximumLineCount: 1
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 7
                visible: view.share.devices.length > 0

                Repeater {
                    model: view.share.devices

                    PillButton {
                        required property var modelData

                        icon: view.deviceIcon(modelData.deviceType)
                        label: modelData.alias
                        onClicked: view.share.sendTo(modelData.fingerprint)
                    }
                }
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2
                visible: view.share.devices.length === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Looking for devices…")
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(11)
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Open LocalSend on the other device")
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(10)
                }
            }

            // A failed attempt hands the files back rather than throwing them
            // away, so the reason belongs next to the devices left to try.
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: text.length > 0
                width: Math.min(300, implicitWidth)
                text: view.share.message
                color: Theme.critical
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
                elide: Text.ElideRight
            }

            // Waiting for a device must not mean waiting out the timer: one
            // click hands the island back.
            RoundButton {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 24
                height: 24
                icon: ["window-close", "dialog-cancel"]
                onClicked: view.share.dismiss()
            }
        }
    }

    // ---- somebody wants to send us something -------------------------------
    Component {
        id: request

        Column {
            spacing: 9

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("%1 wants to send you").arg(view.share.peer)
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: view.summary()
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                maximumLineCount: 1
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                PillButton {
                    accented: true
                    icon: ["dialog-ok", "check"]
                    label: qsTr("Accept")
                    onClicked: view.share.respond(true)
                }

                PillButton {
                    label: qsTr("Decline")
                    tint: Theme.muted
                    onClicked: view.share.respond(false)
                }
            }
        }
    }

    // ---- bytes are moving --------------------------------------------------
    Component {
        id: transfer

        Column {
            spacing: 8

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                IconImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    names: view.phase === "sending"
                           ? ["go-up", "document-send"]
                           : ["go-down", "folder-download"]
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(220, implicitWidth)
                    text: view.phase === "sending"
                          ? qsTr("Sending to %1").arg(view.share.peer)
                          : qsTr("Receiving from %1").arg(view.share.peer)
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(12)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(view.share.progress * 100) + "%"
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(11)
                }

                RoundButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    icon: ["window-close", "dialog-cancel"]
                    onClicked: view.share.dismiss()
                }
            }

            LevelBar {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 260
                height: 4
                value: view.share.progress
                fillColor: Theme.accent
            }
        }
    }

    // ---- it worked, or it did not ------------------------------------------
    Component {
        id: result

        Row {
            spacing: 9

            IconImage {
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                names: view.share.state === "failed"
                       ? ["dialog-error", "error"]
                       : ["dialog-ok", "emblem-success", "check"]
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Text {
                    text: {
                        switch (view.share.state) {
                        case "sent":
                            return qsTr("Sent to %1").arg(view.share.peer)
                        case "received":
                            return qsTr("Saved to your downloads")
                        case "failed":
                            return qsTr("Sharing failed")
                        default:
                            return qsTr("Ready to share")
                        }
                    }
                    color: view.share.state === "failed" ? Theme.critical : Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(12)
                    font.weight: Font.DemiBold
                }

                Text {
                    visible: text.length > 0
                    width: Math.min(280, implicitWidth)
                    text: view.share.message
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(10)
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    // Anywhere on a finished transfer: show what arrived.
    MouseArea {
        anchors.fill: parent
        enabled: view.share.state === "received"
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            view.share.openDestination()
            view.share.dismiss()
        }
    }
}
