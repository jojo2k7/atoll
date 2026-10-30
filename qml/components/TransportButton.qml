// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import Atoll

Item {
    id: button
    property string action: "play"
    property string label: ""
    property bool selected: false
    property real glyphSize: 24
    signal clicked()
    implicitWidth: 40
    implicitHeight: 36
    opacity: enabled ? 1 : .3
    scale: pointer.pressed ? .9 : 1
    Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: if (enabled) clicked()
    ToolTip.visible: pointer.containsMouse && label.length > 0
    ToolTip.text: label
    ToolTip.delay: 650
    Rectangle { anchors.fill: parent; radius: 10; color: pointer.containsMouse ? "#202023" : "transparent"; Behavior on color { ColorAnimation { duration: Theme.fast } } }
    Canvas {
        id: glyph
        width: button.glyphSize; height: width; anchors.centerIn: parent
        onPaint: {
            const c = getContext("2d")
            c.reset(); c.scale(width / 24, height / 24)
            c.fillStyle = button.selected ? "#91adff" : "#f5f5f7"
            c.strokeStyle = c.fillStyle; c.lineWidth = 1.65; c.lineCap = "round"; c.lineJoin = "round"
            function triangle(a,b,d,e,f,g) { c.beginPath(); c.moveTo(a,b); c.lineTo(d,e); c.lineTo(f,g); c.closePath(); c.fill() }
            switch (button.action) {
            case "play": triangle(7,4,20,12,7,20); break
            case "pause": c.fillRect(6,4,4,16); c.fillRect(14,4,4,16); break
            case "previous": triangle(12,5,2,12,12,19); triangle(22,5,12,12,22,19); break
            case "next": triangle(2,5,12,12,2,19); triangle(12,5,22,12,12,19); break
            case "shuffle":
                c.beginPath(); c.moveTo(3,6); c.lineTo(6,6); c.bezierCurveTo(11,6,13,18,18,18); c.lineTo(21,18); c.stroke()
                c.beginPath(); c.moveTo(3,18); c.lineTo(6,18); c.bezierCurveTo(11,18,13,6,18,6); c.lineTo(21,6); c.stroke()
                c.beginPath(); c.moveTo(18,3); c.lineTo(21,6); c.lineTo(18,9); c.moveTo(18,15); c.lineTo(21,18); c.lineTo(18,21); c.stroke(); break
            case "repeat":
                c.beginPath(); c.moveTo(19,8); c.lineTo(6,8); c.quadraticCurveTo(3,8,3,12); c.moveTo(5,16); c.lineTo(18,16); c.quadraticCurveTo(21,16,21,12); c.moveTo(16,5); c.lineTo(19,8); c.lineTo(16,11); c.moveTo(8,13); c.lineTo(5,16); c.lineTo(8,19); c.stroke(); break
            case "output":
                c.strokeRect(5,5,14,12); c.beginPath(); c.moveTo(2,19); c.lineTo(22,19); c.stroke(); break
            }
        }
        Connections { target: button; function onActionChanged() { glyph.requestPaint() } function onSelectedChanged() { glyph.requestPaint() } }
    }
    MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: button.clicked() }
}
