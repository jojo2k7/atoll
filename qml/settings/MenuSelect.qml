// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import Atoll

/**
 * A pop-up menu button, for choices with more values than fit as segments.
 * Options carrying a `group` are listed under that heading.
 */
Rectangle {
    id: control

    /** [{ value: "x", label: "X", group: "" }] */
    property var options: []
    property var current: ""
    property string placeholder: ""

    signal picked(var value)

    readonly property var currentOption: {
        for (const o of options) {
            if (o.value === current) {
                return o
            }
        }
        return null
    }

    implicitWidth: Math.max(150, caption.implicitWidth + 44)
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: hover.hovered || menu.opened ? Skin.controlHover : Skin.control

    Behavior on color {
        ColorAnimation {
            duration: 120
        }
    }

    Text {
        id: caption
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.right: chevrons.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: control.currentOption ? control.currentOption.label : control.placeholder
        color: control.currentOption ? Skin.text : Skin.muted
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    Image {
        id: chevrons
        anchors.right: parent.right
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        sourceSize: Qt.size(28, 28)
        source: Skin.icon("chevrons")
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: menu.opened ? menu.close() : menu.open()
    }

    Popup {
        id: menu

        y: control.height + 4
        width: Math.max(control.width, 220)
        height: Math.min(list.contentHeight + 12, 360)
        padding: 6
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 140; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 90 }
        }

        background: Rectangle {
            radius: 10
            color: "#232327"
            border.width: 1
            border.color: "#1cffffff"
        }

        contentItem: ListView {
            id: list
            clip: true
            model: control.options
            boundsBehavior: Flickable.StopAtBounds
            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Text {
                required property string section

                width: ListView.view.width
                text: section
                visible: section.length > 0
                height: visible ? 26 : 0
                leftPadding: 28
                verticalAlignment: Text.AlignBottom
                bottomPadding: 4
                color: Skin.faint
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            delegate: Rectangle {
                id: item

                required property var modelData

                readonly property bool chosen: modelData.value === control.current

                width: ListView.view.width
                height: 28
                radius: 6
                color: itemHover.hovered ? Skin.accent : "transparent"

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 13
                    height: 13
                    sourceSize: Qt.size(26, 26)
                    source: Skin.icon("check")
                    visible: item.chosen
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: item.modelData.label
                    color: itemHover.hovered ? Skin.accentText : Skin.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                HoverHandler {
                    id: itemHover
                }

                TapHandler {
                    onTapped: {
                        control.picked(item.modelData.value)
                        menu.close()
                    }
                }
            }
        }
    }
}
