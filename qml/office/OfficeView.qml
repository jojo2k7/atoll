// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import Atoll

/**
 * The Claude Code office inside the island (docs/office-v2.md).
 *
 * Everything is drawn at one logical pixel per art pixel into `canvas`
 * (header strip + the 480x270 scene from layout.json), which is rendered
 * through a layer and scaled up by an integer factor with nearest-neighbour
 * sampling, so text in the pixel font and every sprite stay crisp.
 */
FocusScope {
    id: officeView

    signal tick(real dt)
    signal second()

    // ---- geometry ----------------------------------------------------------
    readonly property int sceneW: OfficeArt.sceneWidth
    readonly property int sceneH: OfficeArt.sceneHeight
    readonly property int headerH: 16
    readonly property int pad: 12
    readonly property real screenW: Screen.width > 0 ? Screen.width : 1920
    readonly property real screenH: Screen.height > 0 ? Screen.height : 1080
    readonly property int pixelScale: Math.max(OfficeArt.layout ? OfficeArt.layout.scale.minimum : 2,
                                               Math.floor(Math.min((0.94 * screenW - 2 * pad) / sceneW,
                                                                   (0.86 * screenH - 2 * pad) / (sceneH + headerH))))

    implicitWidth: sceneW * pixelScale + 2 * pad
    implicitHeight: (sceneH + headerH) * pixelScale + 2 * pad

    // ---- interaction state -------------------------------------------------
    property string selectedKey: ""
    property string hoverKey: ""
    property string zoomKey: ""
    property rect zoomFrom: Qt.rect(0, 0, 0, 0)
    property bool systemOpen: false
    property bool questionsHidden: false
    readonly property var questions: App.office.questions
    readonly property bool questionShown: questions.length > 0 && !questionsHidden
    readonly property string highlightKey: questionShown && questionPanel.current
                                           && questionPanel.current.session === App.office.currentSession
                                           ? questionPanel.current.agentKey : ""

    // ---- agents ------------------------------------------------------------
    property var agentMap: ({})
    property var targets: ({})
    readonly property var exitTarget: ({ id: "exit", kind: "exit", facing: "down" })
    readonly property real walkSpeed: 60
    readonly property real walkRate: OfficeArt.layout ? walkSpeed / OfficeArt.layout.characters.walkSpeed : 1
    readonly property bool anyAgents: App.office.agents.length > 0
    readonly property bool anyBusy: App.office.agents.some(a => a.busy)
    property bool firstSync: true
    property string syncedSession: ""
    property real lastSpawn: 0
    property var talkUntil: ({})
    property var planSeen: ({})

    function syncAgents() {
        if (!OfficeArt.ready) {
            return
        }
        if (App.office.currentSession !== syncedSession) {
            syncedSession = App.office.currentSession
            agentModel.clear()
            firstSync = true
        }
        const list = App.office.agents
        const map = {}
        const now = Date.now()
        for (const a of list) {
            map[a.key] = a
            if (a.activity === "talking" && !a.lead) {
                talkUntil[a.key] = now + 6000
            }
            const sc = a.screen
            if (sc && sc.kind === "plan" && planSeen[a.key] !== sc.at) {
                if (planSeen[a.key] !== undefined) {
                    printUntil = OfficeArt.clock + 3.5
                }
                planSeen[a.key] = sc.at
            }
        }
        agentMap = map
        computeTargets()
        const present = {}
        for (let i = 0; i < agentModel.count; ++i) {
            const k = agentModel.get(i).key
            present[k] = true
            if (map[k] === undefined && !agentModel.get(i).gone) {
                agentModel.setProperty(i, "gone", true)
            } else if (map[k] !== undefined && agentModel.get(i).gone) {
                agentModel.setProperty(i, "gone", false)
            }
        }
        for (const a of list) {
            if (present[a.key]) {
                continue
            }
            let delay = 0
            if (!firstSync) {
                delay = Math.max(OfficeArt.clock, lastSpawn + 0.8)
                lastSpawn = delay
            }
            agentModel.append({ key: a.key, gone: false, instant: firstSync, spawnDelay: delay })
        }
        firstSync = false
    }

    function removeAgent(key) {
        for (let i = 0; i < agentModel.count; ++i) {
            if (agentModel.get(i).key === key) {
                if (agentModel.get(i).gone) {
                    agentModel.remove(i)
                }
                return
            }
        }
    }

    function deskTarget(d, lead) {
        return {
            id: lead ? "lead" : "desk:" + d.index, kind: lead ? "lead" : "desk", seat: true,
            node: d.seatNode, x: d.seat.x, y: d.seat.y, z: d.seat.z, facing: d.facing,
            label: d.label, bubble: d.bubble
        }
    }

    function spotTarget(kind, s, pose) {
        const n = OfficeArt.nodes[s.node]
        return {
            id: kind + ":" + s.id, kind: kind, node: s.node, x: s.x ?? n.x, y: s.y ?? n.y,
            facing: s.facing ?? "up", pose: pose ?? s.pose ?? "idle_up", then: s.then
        }
    }

    function computeTargets() {
        if (!OfficeArt.ready) {
            return
        }
        const L = OfficeArt.layout
        const now = Date.now()
        const out = {}
        const agents = App.office.agents.filter(a => a.state !== "leaving")
        const standing = {}
        for (const s of L.standing) {
            standing[s.id] = s
        }
        const bySince = (a, b) => (a.since - b.since) || (a.key < b.key ? -1 : 1)

        const queue = agents.filter(a => a.waitingFor === "user" && a.activity === "asking").sort(bySince)
        queue.forEach((a, i) => {
            const q = L.queue[Math.min(i, L.queue.length - 1)]
            out[a.key] = spotTarget("queue", q, i === 0 ? "question_up" : "idle_up")
        })

        const visitors = agents.filter(a => !out[a.key] && !a.lead
                                       && (a.activity === "talking" || (talkUntil[a.key] ?? 0) > now))
                               .sort(bySince)
        visitors.slice(0, L.lead.visitorSpots.length).forEach((a, i) => {
            out[a.key] = spotTarget("visitor", standing[L.lead.visitorSpots[i]])
        })

        const taken = {}
        const restSeats = ["sofa1", "armchair_l", "sofa0", "armchair_r", "sofa2"]
        const lounge = {}
        for (const l of L.lounge) {
            lounge[l.id] = l
        }
        const resting = []
        for (const a of agents) {
            if (out[a.key]) {
                continue
            }
            const idleFor = now - (a.since ?? now)
            if (a.lead) {
                if (a.state === "idle" && a.waitingFor !== "user" && idleFor > 6000) {
                    resting.push(a)
                } else {
                    out[a.key] = deskTarget(L.lead, true)
                }
            } else if (a.state === "idle" && a.waitingFor !== "user") {
                resting.push(a)
            } else if (a.desk >= 0 && a.desk < L.desks.length) {
                out[a.key] = deskTarget(L.desks[a.desk], false)
            } else {
                const want = { reading: "bookshelf", browsing: "window", planning: "whiteboard",
                               terminal: "server", typing: "kanban" }[a.activity] ?? "printer"
                const order = [want, "whiteboard", "kanban", "bookshelf", "window", "printer", "server", "fridge"]
                const pick = order.find(id => standing[id] && !taken[id])
                if (pick) {
                    taken[pick] = true
                    out[a.key] = spotTarget("spot", standing[pick])
                } else {
                    out[a.key] = deskTarget(L.lead, true)
                }
            }
        }
        resting.sort(bySince)
        let coffeeTaken = false
        let seat = 0
        for (const a of resting) {
            const idleFor = now - (a.since ?? now) - (a.lead ? 6000 : 0)
            if (!coffeeTaken && idleFor < 14000) {
                coffeeTaken = true
                out[a.key] = spotTarget("spot", standing.coffee)
                continue
            }
            if (seat < restSeats.length) {
                const l = lounge[restSeats[seat++]]
                out[a.key] = {
                    id: "lounge:" + l.id, kind: "lounge", seat: true, node: l.node, x: l.x, y: l.y,
                    z: l.z, facing: l.facing, poses: l.poses
                }
                continue
            }
            const pick = ["cooler", "window", "fridge", "bookshelf"].find(id => standing[id] && !taken[id])
            if (pick) {
                taken[pick] = true
                out[a.key] = spotTarget("spot", standing[pick])
            }
        }
        targets = out
    }

    function spotTaken(node) {
        for (const k in targets) {
            if (targets[k].node === node) {
                return true
            }
        }
        return false
    }

    readonly property var deskOccupant: {
        const occ = {}
        for (const a of App.office.agents) {
            if (a.state === "leaving") {
                continue
            }
            if (a.lead) {
                occ[-1] = a
            } else if (a.desk >= 0) {
                occ[a.desk] = a
            }
        }
        return occ
    }

    Connections {
        target: App.office
        function onChanged() {
            officeView.syncAgents()
        }
    }

    Connections {
        target: OfficeArt
        function onReadyChanged() {
            officeView.syncAgents()
        }
    }

    ListModel {
        id: agentModel
    }

    // ---- ambient state -----------------------------------------------------
    property int monitorTick: 0
    property real printUntil: 0
    property real brewUntil: 0
    property real steamUntil: 0
    property bool atCoffee: false
    readonly property bool brewing: atCoffee || OfficeArt.clock < brewUntil
    onBrewingChanged: if (!brewing) {
        steamUntil = OfficeArt.clock + 3
    }
    property real doorOpenUntil: 0
    onQuestionsChanged: {
        if (questions.length < lastQuestionCount) {
            doorOpenUntil = OfficeArt.clock + 2
        }
        if (questions.length > lastQuestionCount) {
            questionsHidden = false
        }
        lastQuestionCount = questions.length
    }
    property int lastQuestionCount: 0

    readonly property string doorFrame: questions.length > 0 ? "ajar"
                                        : (OfficeArt.clock < doorOpenUntil ? "open" : "closed")
    readonly property string lampFrame: questions.length > 0 ? "busy" : (App.office.active ? "free" : "off")

    function lightCondition(when) {
        const m = /^(\w+)(!=|==)(\w+)$/.exec(when)
        if (!m) {
            return true
        }
        const value = m[1] === "julius_door" ? doorFrame : (propFrames[m[1]] ?? "")
        return m[2] === "!=" ? value !== m[3] : value === m[3]
    }

    function kindForActivity(activity) {
        return {
            typing: "code", terminal: "terminal", reading: "doc", browsing: "web", planning: "plan",
            talking: "chat", delegating: "brief", asking: "question", error: "error"
        }[activity] ?? "code"
    }

    function screenKind(a) {
        if (a.activity === "error") {
            return "error"
        }
        if (a.running) {
            return "terminal"
        }
        return a.screen && a.screen.kind ? a.screen.kind : kindForActivity(a.activity)
    }

    readonly property var propFrames: {
        const r = {}
        if (!OfficeArt.ready) {
            return r
        }
        const L = OfficeArt.layout
        const kinds = OfficeArt.sheet("monitor_front").screenKinds
        for (const d of L.desks) {
            const a = deskOccupant[d.index]
            if (d.facing === "up") {
                if (!a) {
                    r[d.monitor] = "off"
                } else {
                    const frames = kinds[screenKind(a)] ?? kinds.tool
                    r[d.monitor] = frames[a.state === "working" ? monitorTick % frames.length : 0]
                }
            } else {
                r[d.monitor] = a ? "on" : "off"
            }
        }
        for (const m of L.lead.monitors) {
            r[m] = deskOccupant[-1] ? "on" : "off"
        }
        r[L.julius.door] = doorFrame
        r[L.julius.lamp] = lampFrame
        r[L.julius.glow] = lampFrame === "off" ? "free" : lampFrame
        return r
    }

    readonly property var propAnims: ({
        coffee_machine: brewing ? "brew" : "idle",
        printer: OfficeArt.clock < printUntil ? "print" : "idle"
    })

    readonly property var lightBoost: ({
        rack_glow: anyBusy ? 1.6 : 1,
        door_glow: lampFrame === "off" ? 0 : 1
    })

    FrameAnimation {
        running: officeView.visible
        onTriggered: {
            const dt = Math.min(frameTime, 0.1)
            OfficeArt.clock += dt
            officeView.tick(dt)
        }
    }

    Timer {
        interval: 600
        running: true
        repeat: true
        onTriggered: officeView.monitorTick++
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            officeView.computeTargets()
            let coffee = false
            for (let i = 0; i < agentRepeater.count; ++i) {
                const it = agentRepeater.itemAt(i)
                if (it && it.phase === "arrived" && it.arrivedAt === "spot:coffee") {
                    coffee = true
                }
            }
            officeView.atCoffee = coffee
            officeView.second()
        }
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: OfficeArt.refreshHour()
    }

    // ---- actions -----------------------------------------------------------
    function deskRect(deskIndex) {
        const L = OfficeArt.layout
        const d = deskIndex < 0 ? L.lead : L.desks[deskIndex]
        const r = d.screenRect ?? d.monitorRect
        return Qt.rect(r[0], r[1], r[2], r[3])
    }

    function openZoom(key) {
        const a = agentMap[key]
        if (!a) {
            return
        }
        selectedKey = key
        let from = Qt.rect(0, 0, 0, 0)
        const it = agentItem(key)
        if (a.lead) {
            from = deskRect(-1)
        } else if (a.desk >= 0 && it && it.seated) {
            from = deskRect(a.desk)
        } else if (it) {
            from = Qt.rect(it.ix - 8, it.iy - 28, 16, 10)
        }
        zoomFrom = from
        zoomKey = key
        if (a.pid > 0) {
            App.office.system.focusPid = a.pid
        }
    }

    function openDesk(deskIndex) {
        const a = deskOccupant[deskIndex]
        if (a) {
            openZoom(a.key)
        }
    }

    function agentItem(key) {
        for (let i = 0; i < agentRepeater.count; ++i) {
            const it = agentRepeater.itemAt(i)
            if (it && it.key === key) {
                return it
            }
        }
        return null
    }

    function openSystem(pid) {
        if (pid > 0) {
            App.office.system.focusPid = pid
        } else {
            const lead = deskOccupant[-1]
            if (lead && lead.pid > 0) {
                App.office.system.focusPid = lead.pid
            }
        }
        systemOpen = true
    }

    function closeTop() {
        if (zoomKey.length > 0) {
            zoomKey = ""
            return true
        }
        if (systemOpen) {
            systemOpen = false
            return true
        }
        if (selectedKey.length > 0) {
            selectedKey = ""
            return true
        }
        return false
    }

    Binding {
        target: App.office.system
        property: "active"
        value: officeView.systemOpen || (officeView.anyBusy && !officeView.systemOpen)
        when: officeView.visible
        restoreMode: Binding.RestoreValue
    }

    Component.onDestruction: App.office.system.active = false

    Connections {
        target: App.office
        function onFocusQuestionChanged() {
            if (App.office.focusQuestion.length > 0) {
                officeView.questionsHidden = false
                questionPanel.focusId(App.office.focusQuestion)
            }
        }
    }

    // ---- keyboard ----------------------------------------------------------
    function refocus() {
        if (zoomKey.length > 0) {
            zoom.forceActiveFocus()
        } else if (questionShown) {
            questionPanel.takeFocus()
        } else {
            keyCatcher.forceActiveFocus()
        }
    }
    onZoomKeyChanged: Qt.callLater(refocus)
    onQuestionShownChanged: Qt.callLater(refocus)
    onSystemOpenChanged: Qt.callLater(refocus)

    Item {
        id: keyCatcher
        focus: true
        Keys.onEscapePressed: {
            if (!officeView.closeTop()) {
                App.office.hide()
            }
        }
    }

    Keys.onEscapePressed: {
        if (!officeView.closeTop()) {
            App.office.hide()
        }
    }

    // ---- morph -------------------------------------------------------------
    property bool revealed: false
    Timer {
        interval: 200
        running: true
        onTriggered: officeView.revealed = true
    }

    Component.onCompleted: {
        OfficeArt.load()
        syncAgents()
        lastQuestionCount = questions.length
        if (App.office.focusQuestion.length > 0) {
            questionPanel.focusId(App.office.focusQuestion)
        }
        Qt.callLater(refocus)
    }

    FontLoader {
        source: "qrc:/qt/qml/Atoll/data/fonts/monogram-extended.ttf"
    }

    Item {
        id: reveal
        width: officeView.sceneW * officeView.pixelScale
        height: (officeView.sceneH + officeView.headerH) * officeView.pixelScale
        x: Math.round((officeView.width - width) / 2)
        y: officeView.pad
        opacity: officeView.revealed ? 1 : 0
        scale: officeView.revealed ? 1 : 0.97
        Behavior on opacity {
            NumberAnimation {
                duration: 240
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: 320
                easing.type: Easing.OutCubic
            }
        }

        Item {
            id: canvas
            width: officeView.sceneW
            height: officeView.sceneH + officeView.headerH
            scale: officeView.pixelScale
            transformOrigin: Item.TopLeft
            visible: OfficeArt.ready
            layer.enabled: true
            layer.smooth: false

            OfficeHeader {
                width: officeView.sceneW
                height: officeView.headerH
                view: officeView
            }

            Item {
                id: scene
                y: officeView.headerH
                width: officeView.sceneW
                height: officeView.sceneH
                clip: true

                // background, floor, wall
                Repeater {
                    model: OfficeArt.ready ? OfficeArt.layout.props.filter(p => p.layer === "background"
                                                                           || p.layer === "floor"
                                                                           || p.layer === "wall") : []
                    OfficeProp {
                        required property var modelData
                        p: modelData
                        view: officeView
                    }
                }

                // the name on the door's plate (plate interior: 3 px in, 2 px down)
                OfficeNamePlate {
                    readonly property var door: OfficeArt.ready
                                                ? OfficeArt.layout.props.find(p => p.id === "julius_door") ?? null
                                                : null
                    visible: door !== null
                    x: door ? door.x + 3 : 0
                    y: door ? door.y + 2 : 0
                }

                // light_floor
                Repeater {
                    model: OfficeArt.ready ? OfficeArt.layout.lights.filter(l => l.layer === "light_floor") : []
                    OfficeProp {
                        required property var modelData
                        p: modelData
                        view: officeView
                        light: true
                    }
                }

                Item {
                    id: shadowLayer
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.props.filter(p => p.layer === "shadow") : []
                        OfficeProp {
                            required property var modelData
                            p: modelData
                            view: officeView
                        }
                    }
                }

                Item {
                    id: objectsLayer
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.props.filter(p => p.layer === "objects") : []
                        OfficeProp {
                            required property var modelData
                            p: modelData
                            view: officeView
                            z: modelData.z
                        }
                    }
                    Repeater {
                        id: agentRepeater
                        model: agentModel
                        OfficeAgent {
                            view: officeView
                        }
                    }
                    Loader {
                        active: OfficeArt.ready
                        // The loader itself is a z-sorted child; the cat moves inside it.
                        z: item ? item.z : 0
                        sourceComponent: OfficeCat {
                            view: officeView
                        }
                    }
                }

                Rectangle {
                    id: nightTint
                    width: officeView.sceneW
                    height: officeView.sceneH
                    color: Qt.rgba(16 / 255, 18 / 255, 48 / 255, 1)
                    opacity: ({ night: 0.59, dusk: 0.38, dawn: 0.22, day: 0 })[OfficeArt.period]
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 3000
                        }
                    }
                }

                Item {
                    id: lightLayer
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.lights.filter(l => l.layer === "light") : []
                        OfficeProp {
                            required property var modelData
                            p: modelData
                            view: officeView
                            light: true
                        }
                    }
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.desks.concat([OfficeArt.layout.lead]) : []
                        Sprite {
                            required property var modelData
                            required property int index
                            readonly property int desk: modelData.index ?? -1
                            readonly property var a: officeView.deskOccupant[desk]
                            readonly property bool typing: a !== undefined && (a.activity === "typing" || a.running
                                                                 || a.activity === "terminal") === true
                            readonly property real noise: {
                                const k = Math.floor(OfficeArt.clock * 3) + index * 17
                                const v = Math.sin(k * 12.9898) * 43758.5453
                                return v - Math.floor(v)
                            }
                            sheet: modelData.glow.sprite
                            px: modelData.glow.x
                            py: modelData.glow.y
                            frame: a ? (OfficeArt.sheet("monitor_glow").kindGlow[officeView.screenKind(a)] ?? "cool") : "cool"
                            visible: a !== undefined
                            opacity: (OfficeArt.dark ? 0.95 : 0.7) * (typing ? 0.85 + 0.15 * noise : 1)
                        }
                    }
                }

                Item {
                    id: ambientLayer
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.props.filter(p => p.layer === "ambient") : []
                        OfficeProp {
                            required property var modelData
                            p: modelData
                            view: officeView
                            visible: modelData.id !== "coffee_steam" || officeView.brewing || OfficeArt.clock < officeView.steamUntil
                        }
                    }
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.lights.filter(l => l.dust !== undefined) : []
                        DustMotes {
                            required property var modelData
                            area: modelData.dust
                            seed: modelData.x
                            visible: OfficeArt.period === "day"
                        }
                    }
                    Repeater {
                        model: OfficeArt.ready ? OfficeArt.layout.lights.filter(l => l.layer === "ambient") : []
                        OfficeProp {
                            required property var modelData
                            p: modelData
                            view: officeView
                            light: true
                        }
                    }
                }

                // Hotspots sit above the characters: a body in front of a
                // monitor does not block it.
                Repeater {
                    model: OfficeArt.ready ? OfficeArt.layout.hotspots.filter(h => h.kind !== "cat") : []
                    MouseArea {
                        required property var modelData
                        readonly property bool live: modelData.kind !== "monitor"
                                                     || officeView.deskOccupant[modelData.desk] !== undefined
                        x: modelData.rect[0]
                        y: modelData.rect[1]
                        width: modelData.rect[2]
                        height: modelData.rect[3]
                        enabled: live
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onContainsMouseChanged: {
                            if (modelData.kind !== "monitor") {
                                return
                            }
                            const a = officeView.deskOccupant[modelData.desk]
                            if (containsMouse && a) {
                                officeView.hoverKey = a.key
                            } else if (a && officeView.hoverKey === a.key) {
                                officeView.hoverKey = ""
                            }
                        }
                        onClicked: {
                            switch (modelData.kind) {
                            case "monitor":
                                officeView.openDesk(modelData.desk)
                                break
                            case "rack":
                                officeView.openSystem(0)
                                break
                            case "coffee":
                                officeView.brewUntil = OfficeArt.clock + 4
                                break
                            case "door":
                                if (officeView.questions.length > 0) {
                                    officeView.questionsHidden = false
                                    questionPanel.takeFocus()
                                } else {
                                    officeView.doorOpenUntil = OfficeArt.clock + 1.5
                                }
                                break
                            }
                        }
                    }
                }

                Item {
                    id: uiLayer
                }

                EmptySign {
                    visible: OfficeArt.ready && !officeView.anyAgents
                    cx: OfficeArt.ready ? OfficeArt.nodes[OfficeArt.layout.lead.approach].x + 45 : 0
                    topY: 118
                }

                // ---- overlays ---------------------------------------------
                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: officeView.zoomKey.length > 0 || officeView.systemOpen ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 180
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: officeView.closeTop()
                        onWheel: wheel => wheel.accepted = true
                    }
                }

                SystemPanel {
                    id: systemPanel
                    view: officeView
                    x: Math.round((officeView.sceneW - width) / 2)
                    y: Math.round((officeView.sceneH - height) / 2)
                    visible: officeView.systemOpen && officeView.zoomKey.length === 0
                }

                QuestionPanel {
                    id: questionPanel
                    view: officeView
                    x: 6
                    y: officeView.sceneH - height - 6
                    visible: officeView.questionShown && officeView.zoomKey.length === 0 && !officeView.systemOpen
                }

                MonitorZoom {
                    id: zoom
                    view: officeView
                    agent: officeView.agentMap[officeView.zoomKey] ?? null
                    from: officeView.zoomFrom
                    open: officeView.zoomKey.length > 0
                }
            }
        }
    }

    property alias shadowLayer: shadowLayer
    property alias lightLayer: lightLayer
    property alias uiLayer: uiLayer
}
