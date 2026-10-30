// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** The pixel office that mirrors running Claude Code sessions. */
SettingsPage {
    id: page

    title: qsTr("Claude Office")
    description: qsTr("A little office where every running Claude Code session has a desk, so you can see at a glance who is working, who is waiting and who has a question.")

    SettingsGroup {
        title: qsTr("Office")

        BoolSetting {
            label: qsTr("Mirror Claude Code sessions")
            description: qsTr("When this is off, hook events are ignored and the office stays empty. Takes effect the next time the island starts.")
            key: "office.enabled"
            defaultValue: true
        }

        TextSetting {
            label: qsTr("Name on the door")
            description: qsTr("The office door is yours. Empty uses your login name; seven letters fit on the plate.")
            key: "office.doorName"
            placeholder: App.userName
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Connect Claude Code")
        footer: qsTr("Registers a silent hook for the events the office listens to. It prints nothing, so it costs no tokens, and it keeps every hook you already have. Run it once per user whose sessions you want to see.")

        SettingRow {
            label: qsTr("Run this in a terminal")
            wide: true
            last: true

            Rectangle {
                width: parent.width
                height: command.implicitHeight + 20
                radius: Skin.radius
                color: Skin.field

                TextEdit {
                    id: command
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    text: "atoll-claude-hooks \"$(command -v atoll)\" ~/.claude/settings.json"
                    readOnly: true
                    selectByMouse: true
                    color: Skin.text
                    selectionColor: Skin.accent
                    selectedTextColor: Skin.accentText
                    font.family: "monospace"
                    font.pixelSize: 12
                    wrapMode: TextEdit.WrapAnywhere
                }
            }
        }
    }
}
