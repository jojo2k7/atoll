// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Dust drifting in a window beam: a handful of motes, each living a few
 * seconds, drifting 2-4 px/s up and sideways and fading in and out. Purely a
 * function of the clock, so nothing to tick.
 */
Item {
    id: dust

    property var area: [0, 0, 0, 0]
    property real seed: 0

    Repeater {
        model: 5

        Sprite {
            required property int index
            readonly property real life: 5 + ((index * 7 + dust.seed) % 4)
            readonly property real offset: index * 1.37 + dust.seed * 0.01
            readonly property real cycle: Math.floor((OfficeArt.clock + offset) / life)
            readonly property real u: (OfficeArt.clock + offset) / life - cycle
            function rnd(k) {
                const v = Math.sin((cycle * 31 + index * 7 + k) * 12.9898 + dust.seed) * 43758.5453
                return v - Math.floor(v)
            }
            readonly property real vx: (rnd(3) - 0.5) * 5
            readonly property real vy: 2 + rnd(4) * 2
            sheet: "dust"
            animation: "twinkle"
            phase: index * 0.4
            px: dust.area[0] + rnd(1) * dust.area[2] + vx * u * life
            py: dust.area[1] + 12 + rnd(2) * (dust.area[3] - 12) - vy * u * life
            opacity: Math.sin(Math.PI * u) * 0.9
        }
    }
}
