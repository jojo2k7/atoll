// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The office cat: dozes or sits at one of `layout.cat.spots`, flicks its
 * tail now and then, and wanders along the walk graph to another spot. At
 * night, or when nobody is in, it prefers its bed. Click it for a heart.
 */
Item {
    id: cat

    required property Item view

    readonly property var info: OfficeArt.layout.cat
    property real fx: OfficeArt.nodes[info.home].x
    property real fy: OfficeArt.nodes[info.home].y
    readonly property int ix: Math.round(fx)
    readonly property int iy: Math.round(fy)

    property var path: []
    property string spot: info.home
    property string pose: "sleep"
    property bool walking: false
    property bool facingLeft: true
    property real restUntil: 30 + Math.random() * 20
    property real flickUntil: 0
    property real heartUntil: 0
    readonly property real clock: OfficeArt.clock

    z: iy

    function spotPose(node) {
        for (const s of info.spots) {
            if (s.node === node) {
                return s.pose
            }
        }
        return "sit"
    }

    function wander() {
        const sleepy = OfficeArt.period === "night" || !view.anyAgents
        let choices = info.spots.filter(s => s.node !== spot)
        if (sleepy) {
            choices = choices.filter(s => s.pose === "sleep")
        }
        // Agents use the coffee spot; the cat only visits while it is free.
        choices = choices.filter(s => !view.spotTaken(s.node))
        if (choices.length === 0) {
            restUntil = clock + 10
            return
        }
        const next = choices[Math.floor(Math.random() * choices.length)]
        path = OfficeArt.walkPoints(fx, fy, next.node)
        spot = next.node
        walking = true
    }

    function step(dt) {
        if (!walking) {
            if (clock > restUntil) {
                wander()
            } else if (pose === "sit" && clock > flickUntil + 4 && Math.random() < dt * 0.15) {
                flickUntil = clock + 0.7
                tail.restart()
            }
            return
        }
        let budget = 26 * dt
        while (budget > 0 && path.length > 0) {
            const p = path[0]
            const dx = p.x - fx
            const dy = p.y - fy
            const d = Math.hypot(dx, dy)
            if (Math.abs(dx) > 0.5) {
                facingLeft = dx < 0
            }
            if (d <= budget) {
                fx = p.x
                fy = p.y
                budget -= d
                path.shift()
            } else {
                fx += dx / d * budget
                fy += dy / d * budget
                budget = 0
            }
        }
        if (path.length === 0) {
            walking = false
            pose = spotPose(spot)
            restUntil = clock + (pose === "sleep" ? 40 : 14) + Math.random() * 25
        }
    }

    Connections {
        target: cat.view
        function onTick(dt) {
            cat.step(dt)
        }
    }

    readonly property string anim: walking ? (facingLeft ? "walk_left" : "walk_right")
                                           : (clock < flickUntil ? "tail_flick" : pose)

    x: ix
    y: iy

    Sprite {
        id: tail
        sheet: cat.info.sprite
        animation: cat.anim
        x: -anchorX
        y: -anchorY
        phase: 0.37
    }

    MouseArea {
        readonly property var rect: {
            for (const h of OfficeArt.layout.hotspots) {
                if (h.kind === "cat") {
                    return h.rect
                }
            }
            return [-9, -13, 18, 14]
        }
        x: rect[0]
        y: rect[1]
        width: rect[2]
        height: rect[3]
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            cat.heartUntil = cat.clock + 2.2
            if (!cat.walking && cat.pose === "sleep") {
                cat.pose = "sit"
                cat.restUntil = cat.clock + 8
            }
        }
    }

    Sprite {
        parent: cat.view.shadowLayer
        sheet: cat.info.shadow
        px: cat.ix
        py: cat.iy
    }

    Sprite {
        parent: cat.view.uiLayer
        sheet: "emote"
        animation: cat.clock < cat.heartUntil ? "heart" : "z"
        px: cat.ix + 4
        py: cat.iy - 12 - (Math.floor(cat.clock * 2) % 2)
        visible: cat.clock < cat.heartUntil || (!cat.walking && cat.pose === "sleep" && cat.spot === cat.info.home
                                                 && Math.floor(cat.clock / 6) % 3 === 0)
    }
}
