// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A field for something that must not be echoed to the room.
 *
 * It never shows a key that is already stored - only that one is, and where it
 * lives. Reading the secret back out into a text box would put it on screen,
 * in the clipboard's reach and in any screen recording, to no purpose: the one
 * useful operation on a stored key is replacing it.
 */
Item {
    id: field

    property bool stored: false
    /** environment | wallet | file | none */
    property string backend: "none"
    property string placeholder: qsTr("Paste your API key")

    signal committed(string value)
    signal cleared()

    implicitWidth: 360
    implicitHeight: row.implicitHeight + (note.visible ? note.implicitHeight + 6 : 0)

    Column {
        anchors.fill: parent
        spacing: 6

        Row {
            id: row
            spacing: 8

            Rectangle {
                width: field.width - (button.width + clearButton.width + 16)
                height: Skin.controlHeight
                radius: Skin.radius
                color: Skin.field
                border.width: input.activeFocus ? 2 : 0
                border.color: Qt.alpha(Skin.accent, 0.8)

                TextInput {
                    id: input
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 34
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    color: Skin.text
                    selectionColor: Skin.accent
                    selectedTextColor: Skin.accentText
                    font.pixelSize: 12
                    echoMode: reveal.checked ? TextInput.Normal : TextInput.Password
                    passwordCharacter: "•"
                    selectByMouse: true
                    // The key is a credential, not prose: no spell checking, no
                    // autocapitalisation, and no predictive text keeping a copy.
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase
                                      | Qt.ImhNoPredictiveText | Qt.ImhHiddenText

                    onAccepted: field.commit()
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 11
                    anchors.verticalCenter: parent.verticalCenter
                    visible: input.text.length === 0 && !input.activeFocus
                    text: field.stored ? qsTr("A key is stored") : field.placeholder
                    color: Skin.muted
                    font.pixelSize: 12
                    opacity: 0.75
                }

                Item {
                    id: reveal
                    property bool checked: false

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    visible: input.text.length > 0

                    IconImage {
                        anchors.fill: parent
                        names: reveal.checked ? ["view-hidden", "hint"] : ["view-visible", "visibility"]
                        opacity: revealHover.hovered ? 1 : 0.6
                    }

                    HoverHandler {
                        id: revealHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: reveal.checked = !reveal.checked
                    }
                }
            }

            PushButton {
                id: button
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Save")
                enabled: input.text.trim().length > 0
                opacity: enabled ? 1 : 0.4
                onClicked: field.commit()
            }

            PushButton {
                id: clearButton
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Remove")
                destructive: true
                visible: field.stored && field.backend !== "environment"
                width: visible ? implicitWidth : 0
                onClicked: field.cleared()
            }
        }

        Text {
            id: note
            width: field.width
            visible: field.stored
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: field.backend === "file" ? "#f0a020" : Skin.muted
            text: {
                switch (field.backend) {
                case "environment":
                    return qsTr("Taken from the environment. Unset the variable to use a stored key instead.")
                case "wallet":
                    return qsTr("Stored in your keyring, encrypted and unlocked with your login.")
                case "file":
                    return qsTr("Stored in a file only you can read. Install libsecret to keep it in your keyring instead.")
                default:
                    return ""
                }
            }
        }
    }

    function commit() {
        const value = input.text.trim()
        if (value.length === 0) {
            return
        }
        input.text = ""
        reveal.checked = false
        field.committed(value)
    }
}
