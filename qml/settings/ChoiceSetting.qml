// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * One of a handful of values: segments while they fit on the row, a pop-up
 * menu once there are more than three.
 */
SettingRow {
    id: setting

    property string key: ""
    property var options: []
    property var defaultValue: ""
    property bool menu: options.length > 3

    Loader {
        sourceComponent: setting.menu ? menuControl : segmentControl
    }

    Component {
        id: segmentControl

        Segmented {
            options: setting.options
            current: Cfg.get(setting.key, setting.defaultValue)
            onPicked: value => Cfg.set(setting.key, value)
        }
    }

    Component {
        id: menuControl

        MenuSelect {
            options: setting.options
            current: Cfg.get(setting.key, setting.defaultValue)
            onPicked: value => Cfg.set(setting.key, value)
        }
    }
}
