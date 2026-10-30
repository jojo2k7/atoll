// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * An agent's monitor, zoomed out of its desk into the big `monitor_big`
 * bezel: the live `screen` in the pixel font, with line numbers and light
 * syntax colours for code, -/+ colours for diffs, and a green terminal with a
 * spinner while a command runs. Scrolls with the wheel and the arrow keys;
 * terminals follow their tail.
 */
FocusScope {
    id: zoom

    required property Item view
    property var agent: null
    property rect from: Qt.rect(0, 0, 0, 0)
    property bool open: false

    readonly property var big: OfficeArt.ready ? OfficeArt.sheet("monitor_big") : undefined
    readonly property var scr: big ? big.screen : [14, 12, 180, 100]
    readonly property int finalX: Math.round((view.sceneW - (big ? big.frameWidth : 208)) / 2)
    readonly property int finalY: Math.max(2, Math.round((view.sceneH - (big ? big.frameHeight : 140)) / 2) - 8)

    property real t: open ? 1 : 0
    Behavior on t {
        NumberAnimation {
            duration: 280
            easing.type: Easing.OutCubic
        }
    }

    readonly property real s0: from.width > 0 ? from.width / scr[2] : 0.2
    readonly property real sc: s0 + (1 - s0) * t
    readonly property real ox: (from.width > 0 ? from.x : finalX + scr[0] + scr[2] * 0.4) * (1 - t) + (finalX + scr[0]) * t
    readonly property real oy: (from.width > 0 ? from.y : finalY + scr[1] + scr[3] * 0.4) * (1 - t) + (finalY + scr[1]) * t

    x: t >= 1 ? finalX : ox - scr[0] * sc
    y: t >= 1 ? finalY : oy - scr[1] * sc
    width: big ? big.frameWidth : 208
    height: 140
    transform: Scale {
        xScale: zoom.t >= 1 ? 1 : zoom.sc
        yScale: zoom.t >= 1 ? 1 : zoom.sc
    }
    visible: t > 0.001 && agent !== null
    opacity: Math.min(1, t * 3)

    // ---- content model ---------------------------------------------------
    readonly property var screen: agent && agent.screen ? agent.screen : ({})
    readonly property string kind: agent ? view.screenKind(agent) : "off"
    readonly property bool terminal: kind === "terminal" || (screen.kind ?? "") === "terminal"
    readonly property bool running: agent ? agent.running === true : false
    readonly property var lines: {
        const text = (screen.text ?? "").replace(/\t/g, "    ").replace(/\r/g, "")
        const out = text.length > 0 ? text.split("\n") : []
        while (out.length > 0 && out[out.length - 1] === "") {
            out.pop()
        }
        return out
    }
    readonly property bool numbered: !terminal && lines.length > 0
                                     && ["code", "diff", "doc", "search"].indexOf(screen.kind ?? "") >= 0
    readonly property int digits: Math.max(2, String(lines.length).length)
    readonly property int textX: numbered ? digits * 6 + 8 : 4
    readonly property int columns: Math.max(8, Math.floor((scr[2] - textX - 9) / 6))
    /** Lines soft-wrapped to the screen: `{line, text, first}`. */
    readonly property var display: {
        const out = []
        for (let i = 0; i < lines.length; ++i) {
            const l = lines[i]
            if (l.length <= columns) {
                out.push({ line: i, text: l, first: true })
                continue
            }
            for (let k = 0; k < l.length; k += columns) {
                out.push({ line: i, text: l.slice(k, k + columns), first: k === 0 })
            }
        }
        return out
    }
    readonly property int rows: 9
    readonly property int totalRows: display.length + (running ? 1 : 0)
    readonly property int maxTop: Math.max(0, totalRows - rows)
    property int scrollTop: 0
    property bool follow: true

    onDisplayChanged: {
        if (follow && terminal) {
            scrollTop = maxTop
        } else {
            scrollTop = Math.min(scrollTop, maxTop)
        }
    }
    onRunningChanged: if (follow && terminal) {
        scrollTop = maxTop
    }
    onAgentChanged: if (!agent) {
        scrollTop = 0
    }
    onOpenChanged: if (open) {
        follow = true
        scrollTop = terminal ? maxTop : 0
    }

    function scrollBy(n) {
        scrollTop = Math.max(0, Math.min(maxTop, scrollTop + n))
        follow = scrollTop >= maxTop
    }

    Keys.onUpPressed: scrollBy(-1)
    Keys.onDownPressed: scrollBy(1)
    Keys.onPressed: event => {
        if (event.key === Qt.Key_PageUp) {
            scrollBy(-rows)
        } else if (event.key === Qt.Key_PageDown || event.key === Qt.Key_Space) {
            scrollBy(rows)
        } else if (event.key === Qt.Key_Home) {
            scrollBy(-totalRows)
        } else if (event.key === Qt.Key_End) {
            scrollBy(totalRows)
        } else if (event.key === Qt.Key_S && zoom.agent && zoom.agent.busy) {
            view.openSystem(zoom.agent.pid)
            view.zoomKey = ""
        } else {
            return
        }
        event.accepted = true
    }
    Keys.onEscapePressed: view.zoomKey = ""

    property real now: Date.now()
    Timer {
        interval: 250
        running: zoom.visible && zoom.running
        repeat: true
        onTriggered: zoom.now = Date.now()
    }

    // ---- colouring ---------------------------------------------------------
    readonly property var keywords: ({
        "def": 1, "class": 1, "return": 1, "if": 1, "elif": 1, "else": 1, "for": 1, "while": 1, "in": 1,
        "import": 1, "from": 1, "as": 1, "with": 1, "try": 1, "except": 1, "finally": 1, "raise": 1,
        "function": 1, "const": 1, "let": 1, "var": 1, "new": 1, "this": 1, "true": 1, "false": 1,
        "null": 1, "None": 1, "True": 1, "False": 1, "fn": 1, "pub": 1, "use": 1, "struct": 1,
        "impl": 1, "mut": 1, "match": 1, "void": 1, "int": 1, "bool": 1, "auto": 1, "static": 1,
        "public": 1, "private": 1, "final": 1, "async": 1, "await": 1, "export": 1, "property": 1,
        "readonly": 1, "required": 1, "signal": 1, "switch": 1, "case": 1, "break": 1, "continue": 1,
        "func": 1, "package": 1, "type": 1, "interface": 1, "extends": 1, "override": 1, "virtual": 1
    })

    function esc(s) {
        return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/ /g, "&nbsp;")
    }

    function span(s, color) {
        return color ? "<font color=\"" + color + "\">" + esc(s) + "</font>" : esc(s)
    }

    function highlight(line) {
        const re = /(\/\/.*$|#.*$)|("(?:[^"\\]|\\.)*"?|'(?:[^'\\]|\\.)*'?|`[^`]*`?)|(\b\d[\d_.]*\b)|([A-Za-z_][A-Za-z0-9_]*)(\s*\()?|([^A-Za-z0-9_"'`#\/]+|\/)/g
        let out = ""
        let m
        while ((m = re.exec(line)) !== null) {
            if (m[0].length === 0) {
                re.lastIndex++
                continue
            }
            if (m[1]) {
                out += span(m[1], "#6f6382")
            } else if (m[2]) {
                out += span(m[2], "#7fd8ae")
            } else if (m[3]) {
                out += span(m[3], "#f6dc6a")
            } else if (m[4]) {
                if (keywords[m[4]]) {
                    out += span(m[4], "#f08a8a")
                } else if (m[5]) {
                    out += span(m[4], "#a8d0ec")
                } else {
                    out += span(m[4])
                }
                if (m[5]) {
                    out += esc(m[5])
                }
            } else {
                out += esc(m[0])
            }
        }
        return out
    }

    function rowHtml(i) {
        if (i >= display.length) {
            if (running && i === display.length) {
                const spin = "|/-\\"[Math.floor(now / 150) % 4]
                return span(spin + " running " + OfficeArt.fmtDuration(now - agent.runningSince), "#f6dc6a")
            }
            return ""
        }
        const line = display[i].text
        const full = lines[display[i].line]
        const sk = screen.kind ?? ""
        if (terminal) {
            return span(line, full.startsWith("$ ") ? "#f2e8dc" : "#7fd8ae")
        }
        if (sk === "diff") {
            if (full.startsWith("-")) {
                return span(line, "#e8806e")
            }
            if (full.startsWith("+")) {
                return span(line, "#7fd8ae")
            }
            return span(line, "#c9bcd0")
        }
        if (sk === "code") {
            return highlight(line)
        }
        return span(line)
    }

    readonly property string kindIcon: ({
        code: "code", diff: "code", terminal: "terminal", doc: "file", search: "search", web: "globe",
        chat: "chat", plan: "plan", brief: "user", question: "question", error: "cross", tool: "bolt"
    })[kind] ?? "file"

    // ---- drawing -----------------------------------------------------------
    Sprite {
        sheet: "monitor_big"
    }

    Item {
        id: screenArea
        x: zoom.scr[0]
        y: zoom.scr[1]
        width: zoom.scr[2]
        height: zoom.scr[3]
        clip: true

        Rectangle {
            anchors.fill: parent
            color: "#05070a"
            visible: zoom.terminal
        }

        Sprite {
            sheet: "icons"
            frame: zoom.kindIcon
            px: 3
            py: 1
        }
        PixelText {
            x: 14
            y: 2
            width: zoom.scr[2] - 18 - nameText.implicitWidth - 6
            elide: Text.ElideRight
            text: zoom.screen.title ?? ""
            color: zoom.terminal ? "#7fd8ae" : "#7fb8f0"
        }
        PixelText {
            id: nameText
            anchors.right: parent.right
            anchors.rightMargin: 4
            y: 2
            text: zoom.running ? OfficeArt.fmtDuration(zoom.now - zoom.agent.runningSince) + " " + (zoom.agent.name ?? "")
                               : (zoom.agent ? zoom.agent.name : "")
            color: zoom.running ? "#f6dc6a" : OfficeArt.dim
        }
        Rectangle {
            x: 2
            y: 11
            width: parent.width - 4
            height: 1
            color: "#2b2440"
        }

        PixelText {
            visible: zoom.lines.length === 0 && !zoom.running
            x: 4
            y: 16
            text: zoom.agent && zoom.agent.detail ? zoom.agent.detail : qsTr("Nothing on screen yet")
            color: OfficeArt.dim
        }

        Repeater {
            model: zoom.rows

            Item {
                required property int index
                readonly property int line: zoom.scrollTop + index
                y: 14 + index * 10
                width: screenArea.width
                height: 10

                PixelText {
                    visible: zoom.numbered && parent.line < zoom.display.length && zoom.display[parent.line].first
                    x: 3
                    y: 0
                    width: zoom.digits * 6
                    horizontalAlignment: Text.AlignRight
                    text: parent.line < zoom.display.length ? String(zoom.display[parent.line].line + 1) : ""
                    color: "#5a5068"
                }
                PixelText {
                    x: zoom.textX
                    y: 0
                    textFormat: Text.StyledText
                    text: zoom.rowHtml(parent.line)
                }
            }
        }

        // Terminal cursor, after the last line once the command is done.
        Rectangle {
            readonly property int row: zoom.display.length - 1 - zoom.scrollTop
            visible: zoom.terminal && !zoom.running && row >= 0 && row < zoom.rows
                     && Math.floor(OfficeArt.clock * 2) % 2 === 0
            x: zoom.textX + (zoom.display.length > 0 ? zoom.display[zoom.display.length - 1].text.length : 0) * 6 + 1
            y: 14 + row * 10 - 1
            width: 5
            height: 8
            color: "#7fd8ae"
        }

        // Scrollbar.
        NineSlice {
            visible: zoom.maxTop > 0
            x: parent.width - 7
            y: 13
            width: 6
            height: parent.height - 15
            sheet: "ui_scroll_track"
        }
        NineSlice {
            readonly property int track: screenArea.height - 15
            visible: zoom.maxTop > 0
            x: parent.width - 7
            height: Math.max(8, Math.round(track * zoom.rows / Math.max(1, zoom.totalRows)))
            y: 13 + Math.round((track - height) * zoom.scrollTop / Math.max(1, zoom.maxTop))
            width: 6
            sheet: "ui_scroll_thumb"
            frame: "normal"
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => zoom.scrollBy(event.angleDelta.y > 0 ? -3 : 3)
        }
    }

    Sprite {
        sheet: "monitor_big_glass"
        px: zoom.scr[0]
        py: zoom.scr[1]
    }

    MouseArea {
        // Swallow clicks on the monitor so the backdrop does not close it.
        anchors.fill: parent
        z: -1
    }

    Row {
        x: zoom.width - width - 2
        y: zoom.scr[1] + zoom.scr[3] + 10
        spacing: 3
        PixelButton {
            visible: zoom.agent !== null && zoom.agent.busy === true
            text: qsTr("System")
            icon: "cpu"
            onClicked: {
                zoom.view.openSystem(zoom.agent.pid)
                zoom.view.zoomKey = ""
            }
        }
        PixelButton {
            icon: "close"
            onClicked: zoom.view.zoomKey = ""
        }
    }
}
