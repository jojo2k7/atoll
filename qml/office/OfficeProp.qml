// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * One entry of layout.json `props` (or `lights`): its sprite at its anchor,
 * with the frame / animation the view overrides at runtime, time-of-day
 * frames that cross-fade over a minute, and clock hands for the clock.
 */
Item {
    id: prop

    required property var p
    required property Item view
    /** For `lights`: day/night opacity multipliers and the `when` condition apply. */
    property bool light: false

    readonly property var frameOverride: view.propFrames[p.id]
    readonly property var animOverride: view.propAnims[p.id]
    readonly property string period: OfficeArt.period

    property string shownPeriod: period
    property string fadingPeriod: ""

    onPeriodChanged: if (p.timeOfDay && period !== shownPeriod) {
        fadingPeriod = shownPeriod
        shownPeriod = period
        fade.restart()
    }

    z: p.z ?? 0
    visible: !light || !p.when || view.lightCondition(p.when)
    opacity: light ? (OfficeArt.dark ? (p.night ?? 1) : (p.day ?? 1)) * (view.lightBoost[p.id] ?? 1) : 1

    Behavior on opacity {
        NumberAnimation {
            duration: 1500
        }
    }

    Sprite {
        sheet: prop.p.sprite
        px: prop.p.x
        py: prop.p.y
        frame: prop.frameOverride !== undefined ? prop.frameOverride
               : (prop.p.timeOfDay ? prop.shownPeriod : prop.p.frame)
        animation: prop.animOverride !== undefined ? prop.animOverride : (prop.p.animation ?? "")
        phase: OfficeArt.hashUnit(prop.p.id) * 13
    }

    Sprite {
        id: previous
        sheet: prop.p.sprite
        px: prop.p.x
        py: prop.p.y
        frame: prop.fadingPeriod
        visible: prop.p.timeOfDay === true && prop.fadingPeriod.length > 0 && opacity > 0
        opacity: 0

        NumberAnimation on opacity {
            id: fade
            running: false
            from: 1
            to: 0
            duration: 60000
        }
    }

    Loader {
        active: prop.p.clock !== undefined
        sourceComponent: Item {
            Sprite {
                sheet: prop.p.clock.hour
                px: prop.p.x
                py: prop.p.y
                frame: Math.floor(OfficeArt.hour) % 12
            }
            Sprite {
                sheet: prop.p.clock.minute
                px: prop.p.x
                py: prop.p.y
                frame: Math.floor(Math.round((OfficeArt.hour % 1) * 60) % 60 / 5)
            }
        }
    }
}
