// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** The `fan` sheet turning at `speed` (0-1) of its manifest fps, smoothly across speed changes. */
Sprite {
    id: fan

    property real speed: 0
    property real turn: 0

    sheet: "fan"
    frame: {
        const a = s && s.animations ? s.animations.spin : undefined
        if (!a) {
            return 0
        }
        return a.frames[Math.floor(turn) % a.frames.length]
    }

    FrameAnimation {
        running: fan.visible && fan.speed > 0.01
        onTriggered: {
            const a = fan.s && fan.s.animations ? fan.s.animations.spin : undefined
            if (a) {
                fan.turn = (fan.turn + Math.min(frameTime, 0.1) * a.fps * Math.min(1.2, fan.speed)) % 1000
            }
        }
    }
}
