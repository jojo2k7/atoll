// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A number with a stepper on either side, all in one capsule. */
Rectangle {
    id: control

    property real value: 0
    property real from: 0
    property real to: 9999
    property real step: 1
    property int decimals: 0
    property string suffix: ""

    signal committed(real value)

    function clamp(candidate) {
        const bounded = Math.max(control.from, Math.min(control.to, candidate))
        return control.decimals > 0 ? Number(bounded.toFixed(control.decimals)) : Math.round(bounded)
    }

    implicitWidth: row.implicitWidth
    implicitHeight: Skin.controlHeight
    radius: Skin.radius
    color: Skin.field

    Row {
        id: row

        StepButton {
            glyph: "−"
            onClicked: control.committed(control.clamp(control.value - control.step))
        }

        InputField {
            id: box
            width: 70
            color: "transparent"
            horizontalAlignment: Text.AlignHCenter
            value: control.decimals > 0 ? control.value.toFixed(control.decimals) + control.suffix
                                        : Math.round(control.value) + control.suffix
            onCommitted: text => {
                const parsed = parseFloat(String(text).replace(control.suffix, ""))
                control.committed(control.clamp(isNaN(parsed) ? control.value : parsed))
            }
        }

        StepButton {
            glyph: "+"
            onClicked: control.committed(control.clamp(control.value + control.step))
        }
    }
}
