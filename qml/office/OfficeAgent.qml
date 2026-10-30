// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * One agent in the office: walks the graph to wherever the view's target
 * assignment sends it, then sits, stands or lounges with a pose picked from
 * what it is doing. The body lives in the z-sorted objects layer; its shadow,
 * glow, emote and name tag are reparented into their own layers.
 */
Item {
    id: agent

    required property string key
    required property bool gone
    required property bool instant
    required property real spawnDelay
    required property Item view

    /** The latest agent object from App.office.agents, kept after it is gone. */
    property var info: ({})
    readonly property var live: view.agentMap[key]
    onLiveChanged: if (live) {
        const prev = info
        info = live
        react(prev, live)
    }

    readonly property var target: view.targets[key] ?? view.exitTarget
    readonly property int look: info.look ?? 1

    // Position in logical pixels (float while walking), drawn rounded.
    property real fx: OfficeArt.layout.entrance.offscreen.x
    property real fy: OfficeArt.layout.entrance.offscreen.y
    readonly property int ix: Math.round(fx)
    readonly property int iy: Math.round(fy)

    property var path: []
    /** "spawning" (waiting to walk in), "walking", "arrived". */
    property string phase: "spawning"
    property string walkingTo: ""
    property string arrivedAt: ""
    property string walkAnim: "walk_up"
    property real holdUntil: 0

    property string gesture: ""
    property real gestureUntil: 0
    property string flash: ""
    property real flashUntil: 0
    property int loungePose: 0
    property real arrivedClock: 0

    readonly property real seed: OfficeArt.hashUnit(key)
    readonly property real speed: view.walkSpeed
    readonly property bool seated: phase === "arrived" && (target.seat ?? false) && arrivedAt === target.id
    readonly property bool hovered: hitArea.containsMouse
    readonly property bool tagShown: view.selectedKey === key || view.hoverKey === key
                                     || view.highlightKey === key
    readonly property real clock: OfficeArt.clock

    z: (seated ? target.z : iy) + 0.001
    visible: OfficeArt.ready

    function react(prev, next) {
        if (!prev || prev.key === undefined) {
            return
        }
        if (next.activity === "error" && prev.activity !== "error") {
            setGesture("facepalm", 2.4)
            setFlash("sweat", 4)
        }
        if (next.lead && prev.state === "working" && next.state === "idle") {
            setGesture("cheer", 2.2)
            setFlash("sparkle", 3)
        }
        if (prev.state !== "leaving" && next.state === "leaving") {
            setFlash("check", 3)
        }
    }

    function setGesture(name, seconds) {
        gesture = name
        gestureUntil = OfficeArt.clock + seconds
    }

    function setFlash(name, seconds) {
        flash = name
        flashUntil = OfficeArt.clock + seconds
    }

    function walkTo(t) {
        walkingTo = t.id
        if (t.kind === "exit") {
            const e = OfficeArt.layout.entrance
            const pts = OfficeArt.walkPoints(fx, fy, e.node)
            pts[pts.length - 1].wave = true
            pts.push({ x: e.offscreen.x, y: e.offscreen.y })
            path = pts
        } else {
            path = OfficeArt.walkPoints(fx, fy, t.node, t.x, t.y)
        }
        phase = "walking"
    }

    function placeAt(t) {
        if (t.kind === "exit") {
            fx = OfficeArt.layout.entrance.offscreen.x
            fy = OfficeArt.layout.entrance.offscreen.y
        } else {
            fx = t.x
            fy = t.y
        }
        arrive(t)
    }

    function arrive(t) {
        phase = "arrived"
        arrivedAt = t.id
        walkingTo = ""
        arrivedClock = OfficeArt.clock
        loungePose = Math.floor(seed * 2)
        if (t.kind === "exit" && agent.gone) {
            view.removeAgent(key)
        }
    }

    function retarget() {
        if (phase === "spawning" || walkingTo === "#entering" || !OfficeArt.ready) {
            return
        }
        if (phase === "arrived" && arrivedAt === target.id) {
            return
        }
        if (phase === "walking" && walkingTo === target.id) {
            return
        }
        walkTo(target)
    }

    onTargetChanged: retarget()
    onGoneChanged: if (gone && (phase === "spawning" || phase === "arrived" && target.kind === "exit")) {
        view.removeAgent(key)
    }

    function step(dt) {
        if (phase === "spawning") {
            if (OfficeArt.clock < spawnDelay) {
                return
            }
            const e = OfficeArt.layout.entrance
            fx = e.offscreen.x
            fy = e.offscreen.y
            path = [{ x: OfficeArt.nodes[e.node].x, y: OfficeArt.nodes[e.node].y, wave: true }]
            walkingTo = "#entering"
            phase = "walking"
            return
        }
        if (phase !== "walking" || OfficeArt.clock < holdUntil) {
            return
        }
        let budget = speed * dt
        while (budget > 0 && path.length > 0) {
            const p = path[0]
            const dx = p.x - fx
            const dy = p.y - fy
            const d = Math.hypot(dx, dy)
            if (d > 0.01) {
                walkAnim = Math.abs(dx) >= Math.abs(dy) ? (dx > 0 ? "walk_right" : "walk_left")
                                                        : (dy > 0 ? "walk_down" : "walk_up")
            }
            if (d <= budget) {
                fx = p.x
                fy = p.y
                budget -= d
                path.shift()
                if (p.wave && walkingTo !== "#entering" && path.length > 0) {
                    // On the way out: a wave at the door, then off screen.
                    setGesture("wave", 1.0)
                    holdUntil = OfficeArt.clock + 1.0
                    return
                }
            } else {
                fx += dx / d * budget
                fy += dy / d * budget
                budget = 0
            }
        }
        if (path.length === 0) {
            if (walkingTo === "#entering") {
                setGesture("wave", 1.0)
                holdUntil = OfficeArt.clock + 1.0
                walkTo(target)
                return
            }
            arrive(target)
        }
    }

    Connections {
        target: agent.view
        function onTick(dt) {
            agent.step(dt)
        }
        function onSecond() {
            if (agent.phase === "arrived" && agent.target.kind === "lounge"
                    && Math.random() < 0.08) {
                agent.loungePose = 1 - agent.loungePose
            }
        }
    }

    Component.onCompleted: {
        if (instant && OfficeArt.ready) {
            info = live ?? {}
            placeAt(target)
        } else if (live) {
            info = live
        }
    }

    // ---- pose --------------------------------------------------------------
    readonly property bool gestureOn: gesture.length > 0 && clock < gestureUntil
    readonly property string facing: target.facing ?? "down"

    function seatedAnim() {
        const f = facing
        if (gestureOn && (gesture === "cheer" || gesture === "facepalm")) {
            return "sit_" + gesture + "_" + f
        }
        const a = info.activity ?? "idle"
        if (info.waitingFor === "user") {
            return "sit_raise_" + f
        }
        if (info.running) {
            return "sit_type_" + f
        }
        switch (a) {
        case "typing":
        case "terminal":
            return "sit_type_" + f
        case "reading":
        case "browsing":
            return "sit_read_" + f
        case "thinking":
        case "planning":
        case "archiving":
            return "sit_think_" + f
        case "delegating":
        case "talking":
            return f === "down" && info.lead ? "sit_talk_down" : "sit_think_" + f
        default:
            return "sit_idle_" + f
        }
    }

    // A target without a pose for this moment falls back to standing.
    readonly property string anim: chooseAnim() ?? "idle_down"

    function chooseAnim() {
        if (phase === "spawning") {
            return "idle_up"
        }
        if (gestureOn && gesture === "wave") {
            return "wave"
        }
        if (phase === "walking") {
            return OfficeArt.clock < holdUntil ? "idle_down" : walkAnim
        }
        const t = target
        if (arrivedAt !== t.id) {
            return "idle_down"
        }
        switch (t.kind) {
        case "desk":
        case "lead":
            return seatedAnim()
        case "lounge": {
            const idleFor = Date.now() - (info.since ?? Date.now())
            if (idleFor > 90000 || OfficeArt.period === "night" && idleFor > 30000) {
                return t.poses[2]
            }
            return t.poses[loungePose]
        }
        default:
            if (gestureOn && (gesture === "cheer" || gesture === "facepalm")) {
                return gesture
            }
            if (t.then && clock - arrivedClock > 3 + seed * 2) {
                // "drink" loops; "stretch" plays once, then back to the pose.
                if (t.then !== "stretch" || clock - arrivedClock < 5 + seed * 2 + 1.7) {
                    return t.then
                }
            }
            return t.pose
        }
    }
    onAnimChanged: {
        body.startedAt = OfficeArt.clock
        body.phase = seed * 7
        if (anim === "facepalm" || anim === "stretch" || anim.indexOf("facepalm") >= 0) {
            body.phase = 0
        }
    }

    // ---- emote -------------------------------------------------------------
    readonly property string emote: {
        if (phase === "spawning") {
            return ""
        }
        if (flash.length > 0 && clock < flashUntil) {
            return flash
        }
        if (phase === "walking" && target.kind !== "visitor") {
            return ""
        }
        if (info.waitingFor === "user") {
            return "question"
        }
        switch (target.kind) {
        case "visitor":
            return "dots"
        case "lounge":
            return anim.indexOf("sleep") >= 0 ? "z" : (anim.indexOf("sip") >= 0 ? "coffee"
                   : (target.id === "lounge:sofa2" ? "music" : ""))
        case "spot":
            return anim === "drink" ? "coffee" : ""
        }
        if (info.activity === "planning") {
            return "bulb"
        }
        if (info.lead && info.activity === "thinking") {
            return "dots"
        }
        return ""
    }

    readonly property point bubblePos: seated && target.bubble
                                       ? Qt.point(target.bubble.x, target.bubble.y)
                                       : Qt.point(ix + OfficeArt.layout.characters.bubbleOffset[0],
                                                  iy + OfficeArt.layout.characters.bubbleOffset[1])
    readonly property point labelPos: seated && target.label
                                      ? Qt.point(target.label.x, target.label.y)
                                      : Qt.point(ix + OfficeArt.layout.characters.labelOffset[0],
                                                 iy + OfficeArt.layout.characters.labelOffset[1])

    // ---- drawing -----------------------------------------------------------
    Sprite {
        id: body
        sheet: "characters"
        variant: agent.look
        animation: agent.anim
        rate: agent.phase === "walking" ? agent.view.walkRate : 1
        px: 0
        py: 0
        x: -anchorX
        y: -anchorY
    }

    // The item sits at the feet so `z` and hit-testing follow them.
    x: ix
    y: iy

    MouseArea {
        id: hitArea
        x: -9
        y: -26
        width: 18
        height: 27
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onContainsMouseChanged: {
            if (containsMouse) {
                agent.view.hoverKey = agent.key
            } else if (agent.view.hoverKey === agent.key) {
                agent.view.hoverKey = ""
            }
        }
        onClicked: agent.view.openZoom(agent.key)
    }

    Sprite {
        parent: agent.view.shadowLayer
        sheet: OfficeArt.layout.characters.shadow
        px: agent.ix
        py: agent.iy
        visible: !agent.seated && agent.phase !== "spawning"
    }

    Sprite {
        id: spotlight
        parent: agent.view.lightLayer
        sheet: "pool_small"
        px: agent.ix
        py: agent.iy
        visible: agent.view.highlightKey === agent.key
        opacity: 0.55 + 0.45 * Math.abs(Math.sin(agent.clock * 2.4))
    }

    Sprite {
        parent: agent.view.uiLayer
        sheet: "emote"
        animation: agent.emote
        phase: agent.seed * 3
        px: agent.bubblePos.x
        py: agent.bubblePos.y - (Math.floor(agent.clock * 2 + agent.seed * 2) % 2)
        visible: agent.emote.length > 0 && !agent.tagShown
    }

    NameTag {
        parent: agent.view.uiLayer
        visible: agent.tagShown && agent.phase !== "spawning"
        text: agent.info.name ?? ""
        cx: agent.labelPos.x
        topY: agent.labelPos.y
        status: agent.info.activity === "error" ? "hot"
              : agent.info.waitingFor === "user" ? "warn"
              : agent.info.state === "working" ? "ok" : "info"
    }
}
