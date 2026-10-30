// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * The office's art and scene data (data/office/manifest.json and layout.json,
 * see docs/office-art.md), loaded once, plus the helpers everything in the
 * scene shares: frame lookup, the walk graph and its path finder, the shared
 * animation clock and the time of day.
 */
QtObject {
    id: art

    readonly property string base: "qrc:/qt/qml/Atoll/data/office/"

    property var sheets: null
    property var layout: null
    readonly property bool ready: sheets !== null && layout !== null

    readonly property int sceneWidth: layout ? layout.size[0] : 480
    readonly property int sceneHeight: layout ? layout.size[1] : 270

    /** Seconds; advanced by whichever office view is on screen. */
    property real clock: 0

    /** Hour of day as a fraction (14.5 = 14:30); `office.debugHour` in the config overrides it. */
    property real realHour: 12
    readonly property real hour: {
        const forced = Number(Cfg.get("office.debugHour", -1))
        return forced >= 0 ? forced : realHour
    }
    readonly property string period: hour >= 5 && hour < 8 ? "dawn"
                                   : hour >= 8 && hour < 18 ? "day"
                                   : hour >= 18 && hour < 21 ? "dusk" : "night"
    readonly property bool dark: period === "dusk" || period === "night"

    readonly property color text: "#f2e8dc"
    readonly property color dim: "#9c8ea6"
    readonly property color accent: "#f0a24a"
    readonly property color panel: "#251c2c"
    readonly property color ok: "#7fd8ae"
    readonly property color warn: "#f6dc6a"
    readonly property color hot: "#d0503f"
    readonly property color info: "#a8d0ec"
    readonly property color outline: "#1e1724"

    property var nodes: ({})
    property var adjacency: ({})
    property var propsById: ({})

    function refreshHour() {
        const d = new Date()
        realHour = d.getHours() + d.getMinutes() / 60
    }

    function load() {
        if (ready) {
            return
        }
        fetch("manifest.json", data => art.sheets = data.sheets)
        fetch("layout.json", data => {
            const nodes = {}
            const adjacency = {}
            for (const n of data.graph.nodes) {
                nodes[n.id] = n
                adjacency[n.id] = []
            }
            for (const e of data.graph.edges) {
                adjacency[e[0]].push(e[1])
                adjacency[e[1]].push(e[0])
            }
            const props = {}
            for (const p of data.props) {
                props[p.id] = p
            }
            art.nodes = nodes
            art.adjacency = adjacency
            art.propsById = props
            art.layout = data
        })
    }

    function fetch(file, done) {
        const data = App.office.sceneData(file)
        if (!data) {
            console.warn("atoll: office scene file", file, "missing")
            return
        }
        done(data)
    }

    function sheet(name) {
        return sheets ? sheets[name] : undefined
    }

    function frameIndex(s, frame) {
        if (frame === undefined || frame === null || frame === "") {
            return 0
        }
        if (typeof frame === "string") {
            const named = s.frameNames ? s.frameNames[frame] : undefined
            return named === undefined ? 0 : named
        }
        return frame
    }

    /** The frame index an animation shows `t` seconds after it started. */
    function animFrame(s, name, t) {
        const a = s.animations ? s.animations[name] : undefined
        if (!a) {
            return -1
        }
        const n = a.frames.length
        let k = Math.floor(t * a.fps)
        k = a.loop ? ((k % n) + n) % n : Math.max(0, Math.min(k, n - 1))
        return a.frames[k]
    }

    function animDuration(sheetName, name) {
        const s = sheet(sheetName)
        const a = s && s.animations ? s.animations[name] : undefined
        return a ? a.frames.length / a.fps : 0
    }

    function nearestNode(x, y) {
        let best = ""
        let bestD = 1e12
        for (const id in nodes) {
            const n = nodes[id]
            const d = (n.x - x) * (n.x - x) + (n.y - y) * (n.y - y)
            if (d < bestD) {
                bestD = d
                best = id
            }
        }
        return best
    }

    /** Shortest path (node ids, Euclidean edge lengths) on the walk graph. */
    function route(from, to) {
        if (from === to) {
            return [from]
        }
        const dist = {}
        const prev = {}
        const open = [from]
        dist[from] = 0
        const h = id => Math.hypot(nodes[id].x - nodes[to].x, nodes[id].y - nodes[to].y)
        const closed = {}
        while (open.length > 0) {
            let bi = 0
            for (let i = 1; i < open.length; ++i) {
                if (dist[open[i]] + h(open[i]) < dist[open[bi]] + h(open[bi])) {
                    bi = i
                }
            }
            const cur = open.splice(bi, 1)[0]
            if (cur === to) {
                break
            }
            closed[cur] = true
            for (const nb of adjacency[cur]) {
                if (closed[nb]) {
                    continue
                }
                const d = dist[cur] + Math.hypot(nodes[nb].x - nodes[cur].x, nodes[nb].y - nodes[cur].y)
                if (dist[nb] === undefined || d < dist[nb]) {
                    dist[nb] = d
                    prev[nb] = cur
                    if (open.indexOf(nb) < 0) {
                        open.push(nb)
                    }
                }
            }
        }
        if (dist[to] === undefined) {
            return [from, to]
        }
        const path = [to]
        while (path[0] !== from) {
            path.unshift(prev[path[0]])
        }
        return path
    }

    /** Waypoints from (x, y) to a node, then on to (tx, ty) when given. */
    function walkPoints(x, y, toNode, tx, ty) {
        const start = nearestNode(x, y)
        const ids = route(start, toNode)
        const pts = []
        for (const id of ids) {
            pts.push({ x: nodes[id].x, y: nodes[id].y })
        }
        // Already walking along the first edge: do not turn back to its start.
        if (pts.length > 1) {
            const a = pts[0], b = pts[1]
            const len2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)
            const u = len2 > 0 ? ((x - a.x) * (b.x - a.x) + (y - a.y) * (b.y - a.y)) / len2 : 0
            if (u > 0 && u < 1 && Math.hypot(a.x + u * (b.x - a.x) - x, a.y + u * (b.y - a.y) - y) < 3) {
                pts.shift()
            }
        }
        if (tx !== undefined && (tx !== pts[pts.length - 1].x || ty !== pts[pts.length - 1].y)) {
            pts.push({ x: tx, y: ty })
        }
        return pts
    }

    /** Stable pseudo-random number in [0, 1) for a string, for per-agent phases. */
    function hashUnit(s) {
        let h = 2166136261
        for (let i = 0; i < s.length; ++i) {
            h ^= s.charCodeAt(i)
            h = Math.imul(h, 16777619) >>> 0
        }
        return (h % 100000) / 100000
    }

    function fmtBytes(b) {
        if (b === undefined || b < 0) {
            return "-"
        }
        if (b < 1024) {
            return Math.round(b) + "B"
        }
        if (b < 1024 * 1024) {
            return Math.round(b / 1024) + "K"
        }
        if (b < 1024 * 1024 * 1024) {
            const m = b / 1048576
            return (m < 10 ? m.toFixed(1) : Math.round(m)) + "M"
        }
        const g = b / 1073741824
        return (g < 10 ? g.toFixed(1) : Math.round(g)) + "G"
    }

    function fmtDuration(ms) {
        const s = Math.max(0, Math.floor(ms / 1000))
        if (s < 60) {
            return s + "s"
        }
        const m = Math.floor(s / 60)
        if (m < 60) {
            return m + "m" + (s % 60 < 10 ? "0" : "") + (s % 60) + "s"
        }
        return Math.floor(m / 60) + "h" + (m % 60 < 10 ? "0" : "") + (m % 60) + "m"
    }

    Component.onCompleted: {
        refreshHour()
        load()
    }
}
