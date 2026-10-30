// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A `ui_tag` name plate with a status LED, centred on `cx`, its top at `topY`. */
Item {
    id: tag

    property string text: ""
    property int cx: 0
    property int topY: 0
    /** `ui_led_dot` frame: ok, warn, hot, info, off. */
    property string status: "ok"

    width: label.implicitWidth + 10
    height: 11
    x: Math.round(cx - width / 2)
    y: topY

    NineSlice {
        anchors.fill: parent
        sheet: "ui_tag"
    }

    Sprite {
        sheet: "ui_led_dot"
        frame: tag.status
        px: 3
        py: 4
    }

    PixelText {
        id: label
        x: 7
        y: 2
        text: tag.text
    }
}
