// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

Rectangle {
    id: tile
    readonly property var session: App.focus.state
    signal clicked()
    implicitWidth: 148
    implicitHeight: 48
    radius: 15
    color: mouse.containsMouse ? "#27272b" : "#1c1c1f"
    scale: mouse.pressed ? 0.97 : 1
    Behavior on color { ColorAnimation { duration: Theme.fast } }
    Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
    Accessible.role: Accessible.Button
    Accessible.name: "Focus " + (session.active ? (session.paused ? "paused" : "running") : "off")
    Accessible.onPressAction: tile.clicked()

    Canvas {
        x: 13; anchors.verticalCenter: parent.verticalCenter
        width: 22; height: 22
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#d4c3ff"
            ctx.beginPath(); ctx.arc(11, 11, 9, 0, Math.PI * 2); ctx.fill()
            ctx.globalCompositeOperation = "destination-out"
            ctx.beginPath(); ctx.arc(16, 6, 9, 0, Math.PI * 2); ctx.fill()
            ctx.globalCompositeOperation = "source-over"
        }
    }
    Column {
        x: 45; anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 61; spacing: 2
        Text { text: "Focus"; color: "#f5f5f7"; font.pixelSize: 12; font.weight: Font.DemiBold }
        Text {
            width: parent.width
            text: !tile.session.active ? "Off" : tile.session.paused ? "Paused" : (tile.session.phase === "break" ? "Break · " : "") + Math.ceil(tile.session.remaining / 60) + " min"
            color: "#99999f"; font.pixelSize: 10; elide: Text.ElideRight
        }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: "#7d7d85"; font.pixelSize: 18 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: tile.clicked() }
}
