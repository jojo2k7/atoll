// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Atoll

Column {
    id: picker
    property bool expanded: false
    property bool showHeader: true
    property bool showVirtual: false
    readonly property var visibleOutputs: App.audio.outputs.filter(o => o.physical || o.active || showVirtual)
    readonly property bool hasVirtual: App.audio.outputs.some(o => !o.physical && !o.active)
    spacing: 6
    visible: showHeader || expanded

    Button {
        id: headerButton
        visible: picker.showHeader
        width: parent.width
        text: App.audio.currentName
        Accessible.name: qsTr("Audio output: %1").arg(text)
        onClicked: { picker.expanded = !picker.expanded; if (picker.expanded) App.audio.refresh() }
        contentItem: Text {
            text: (App.audio.bluetoothActive ? "♬  " : "◉  ") + headerButton.text + (picker.expanded ? "  ▴" : "  ▾")
            color: Theme.foreground; elide: Text.ElideRight
            font.pixelSize: 12; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { radius: 9; color: Qt.rgba(1, 1, 1, .08) }
    }
    Column {
        visible: picker.expanded
        width: parent.width; spacing: 6
        Flickable {
            width: parent.width
            height: Math.min(220, devices.implicitHeight)
            contentHeight: devices.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Column {
                id: devices
                width: parent.width; spacing: 4
                Repeater {
                    model: picker.visibleOutputs
                    delegate: Button {
                        id: outputButton
                        required property var modelData
                        width: devices.width; height: 44
                        enabled: !App.audio.busy && !App.lock.locked
                        Accessible.name: outputButton.modelData.label + (outputButton.modelData.active ? qsTr(", selected") : "")
                        onClicked: App.audio.selectOutput(outputButton.modelData.id)
                        contentItem: Column {
                            spacing: 2
                            Text {
                                width: parent.width
                                text: (outputButton.modelData.active ? "✓  " : "") + outputButton.modelData.label
                                color: Theme.foreground; elide: Text.ElideRight; font.pixelSize: 12
                            }
                            Text {
                                text: outputButton.modelData.active ? qsTr("Current output") : outputButton.modelData.bluetooth && !outputButton.modelData.connected ? qsTr("Connect to this computer") : qsTr("Use this output")
                                color: Theme.muted; font.pixelSize: 10
                            }
                        }
                        background: Rectangle {
                            radius: 9
                            color: outputButton.modelData.active ? Qt.rgba(1, 1, 1, .16) : Qt.rgba(1, 1, 1, .06)
                        }
                    }
                }
            }
        }
        Button {
            id: moreButton
            width: parent.width
            visible: picker.hasVirtual
            text: picker.showVirtual ? qsTr("Fewer outputs") : qsTr("More outputs")
            onClicked: picker.showVirtual = !picker.showVirtual
            contentItem: Text { text: moreButton.text; color: Theme.muted; font.pixelSize: 11 }
            background: Rectangle { radius: 9; color: "transparent" }
        }
        Button {
            id: releaseButton
            width: parent.width
            visible: App.audio.bluetoothActive
            enabled: !App.audio.busy && !App.lock.locked
            text: qsTr("Release for another device")
            onClicked: App.audio.releaseBluetooth()
            contentItem: Text { text: releaseButton.text; color: Theme.foreground; font.pixelSize: 11; elide: Text.ElideRight }
            background: Rectangle { radius: 9; color: Qt.rgba(1, 1, 1, .08) }
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: App.audio.message || (App.audio.outputs.length === 0 ? qsTr("No audio outputs found") : "")
            color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
            Accessible.role: Accessible.StaticText
        }
    }
}
