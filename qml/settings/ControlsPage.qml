// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Every way of telling the island to do something, on one page: what each
 * pointer gesture does, how the hover behaves, the keyboard shortcuts, and
 * the clicks inside the island's own views. See docs/controls.md.
 */
SettingsPage {
    id: page

    title: qsTr("Controls")
    description: qsTr("Give every click, scroll and key combination the job you want. Any trigger can carry any action.")

    function actionOptions(withPeek) {
        return Actions.all.filter(a => withPeek || a.id !== "peek")
                          .map(a => ({ value: a.id, label: a.label, group: a.group }))
    }

    readonly property bool shortcutsAvailable: App.shortcuts ? App.shortcuts.available : false

    SettingsGroup {
        title: qsTr("Pointer")
        footer: qsTr("While the island shows something with buttons of its own - the assistant, the office, a share offer - those buttons win.")

        Repeater {
            model: Actions.triggers.filter(t => t.key !== "hover")

            delegate: SettingRow {
                required property var modelData
                required property int index

                label: modelData.label
                last: index === Actions.triggers.length - 2

                MenuSelect {
                    width: 240
                    options: page.actionOptions(false)
                    current: Cfg.get("controls." + modelData.key, modelData.fallback)
                    onPicked: value => Cfg.set("controls." + modelData.key, value)
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Hover")

        SettingRow {
            label: qsTr("Rest the pointer")
            description: qsTr("Peek shows what is playing; any other action fires once the pointer has rested.")

            MenuSelect {
                width: 240
                options: page.actionOptions(true)
                current: Cfg.get("controls.hover", "peek")
                onPicked: value => Cfg.set("controls.hover", value)
            }
        }

        NumberSetting {
            label: qsTr("Resting time")
            key: "controls.hoverDelay"
            defaultValue: 180
            from: 0
            to: 2000
            step: 10
            suffix: " ms"
        }

        BoolSetting {
            label: qsTr("Fold up when the pointer leaves")
            key: "controls.collapseOnLeave"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Timing")

        NumberSetting {
            label: qsTr("Press and hold for")
            key: "controls.longPressMs"
            defaultValue: 450
            from: 150
            to: 1500
            step: 50
            suffix: " ms"
        }

        NumberSetting {
            label: qsTr("Volume per scroll step")
            key: "controls.volumeStep"
            defaultValue: 5
            from: 1
            to: 25
            suffix: "%"
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Keyboard shortcuts")
        footer: page.shortcutsAvailable
                ? qsTr("These work in every app. They also appear under System Settings → Shortcuts → Atoll. Backspace clears a shortcut while recording.")
                : qsTr("This build has no global shortcut support. Bind `atollctl action <name>` to a key in System Settings → Shortcuts instead.")

        Repeater {
            model: page.shortcutsAvailable ? Actions.shortcutActions : []

            delegate: SettingRow {
                required property var modelData
                required property int index

                label: modelData.label
                last: index === Actions.shortcutActions.length - 1

                ShortcutField {
                    width: 180
                    value: Cfg.get("controls.shortcuts." + modelData.id, "")
                    onCommitted: sequence => Cfg.set("controls.shortcuts." + modelData.id, sequence)
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Inside the island")

        BoolSetting {
            label: qsTr("Clicking a notification opens its app")
            description: qsTr("Raises the application that sent it, or starts one if none is running.")
            key: "notifications.openOnClick"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Clicking a lyric line seeks to it")
            key: "lyrics.seekOnClick"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Answer agent questions on the island")
            description: qsTr("When Claude Code asks something, the office opens with the question in front.")
            key: "office.askInIsland"
            defaultValue: true
            last: true
        }
    }
}
