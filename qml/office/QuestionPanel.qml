// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * An agent's AskUserQuestion, answered on the island (docs/office-v2.md
 * section 3). Mirrors the CLI picker: a tab per question plus "Submit" when
 * there are several, numbered options with dimmed descriptions, a final
 * "Type something." row that becomes a text field; arrows, Enter, digits,
 * Space (multi-select), Tab / Left / Right, and Esc to answer in the terminal.
 */
FocusScope {
    id: panel

    required property Item view

    readonly property var asks: view.questions
    property int askIndex: 0
    readonly property var current: asks.length > 0 ? asks[Math.min(askIndex, asks.length - 1)] : null
    readonly property var qs: current ? current.questions ?? [] : []
    readonly property bool several: qs.length > 1
    readonly property bool onSubmit: several && tab === qs.length
    readonly property var q: !onSubmit && tab < qs.length ? qs[tab] : null
    readonly property var options: q && q.options ? q.options : []
    readonly property bool multi: q ? q.multiSelect === true : false
    readonly property int customRow: options.length

    property string stateFor: ""
    property int tab: 0
    property int cursor: 0
    property var picks: []
    property var customs: []
    property var answered: []
    property bool editing: false

    width: 252
    height: body.height + 12

    onCurrentChanged: {
        const id = current ? current.id : ""
        if (id === stateFor) {
            return
        }
        stateFor = id
        tab = 0
        cursor = 0
        editing = false
        picks = qs.map(() => [])
        customs = qs.map(() => "")
        answered = qs.map(() => false)
    }
    onTabChanged: {
        cursor = 0
        editing = false
    }

    function focusId(id) {
        for (let i = 0; i < asks.length; ++i) {
            if (asks[i].id === id) {
                askIndex = i
            }
        }
        takeFocus()
    }

    function takeFocus() {
        if (editing) {
            field.forceActiveFocus()
        } else {
            panel.forceActiveFocus()
        }
    }

    function answerText(i) {
        if (customs[i] && customs[i].length > 0 && (picks[i].length === 0 || !qs[i].multiSelect)) {
            return customs[i]
        }
        const labels = picks[i].slice()
        if (qs[i].multiSelect && customs[i] && customs[i].length > 0) {
            labels.push(customs[i])
        }
        return labels.join(", ")
    }

    function setAnswered(i, value) {
        const a = answered.slice()
        a[i] = value
        answered = a
    }

    function advance() {
        if (!several) {
            submit()
            return
        }
        for (let k = 1; k <= qs.length; ++k) {
            const i = (tab + k) % qs.length
            if (!answered[i]) {
                tab = i
                return
            }
        }
        tab = qs.length
    }

    function choose(row) {
        if (!q) {
            return
        }
        if (row === customRow) {
            cursor = row
            editing = true
            field.forceActiveFocus()
            return
        }
        const label = options[row].label
        if (multi) {
            toggle(row)
            return
        }
        const p = picks.slice()
        p[tab] = [label]
        picks = p
        const c = customs.slice()
        c[tab] = ""
        customs = c
        setAnswered(tab, true)
        advance()
    }

    function toggle(row) {
        if (!q || row >= options.length) {
            return
        }
        const label = options[row].label
        const p = picks.slice()
        const mine = p[tab].slice()
        const at = mine.indexOf(label)
        if (at >= 0) {
            mine.splice(at, 1)
        } else {
            mine.push(label)
        }
        p[tab] = mine
        picks = p
    }

    function confirm() {
        if (onSubmit) {
            submit()
            return
        }
        if (cursor === customRow && !editing) {
            choose(customRow)
            return
        }
        if (multi) {
            if (picks[tab].length === 0 && (customs[tab] ?? "").length === 0 && cursor < customRow) {
                toggle(cursor)
            }
            setAnswered(tab, answerText(tab).length > 0)
            if (answered[tab]) {
                advance()
            }
            return
        }
        choose(cursor)
    }

    function acceptCustom() {
        if ((customs[tab] ?? "").length === 0) {
            return
        }
        if (!multi) {
            const p = picks.slice()
            p[tab] = []
            picks = p
        }
        setAnswered(tab, true)
        editing = false
        panel.forceActiveFocus()
        advance()
    }

    function submit() {
        if (!current) {
            return
        }
        for (let i = 0; i < qs.length; ++i) {
            if (answerText(i).length === 0) {
                tab = i
                return
            }
        }
        const map = {}
        for (let i = 0; i < qs.length; ++i) {
            map[qs[i].question] = answerText(i)
        }
        App.office.answerQuestion(current.id, map)
    }

    function cancel() {
        if (current) {
            App.office.cancelQuestion(current.id)
        }
    }

    function moveTab(d) {
        if (!several) {
            return
        }
        tab = (tab + d + qs.length + 1) % (qs.length + 1)
        panel.forceActiveFocus()
    }

    Keys.onPressed: event => {
        if (handleKey(event)) {
            event.accepted = true
        }
    }

    function handleKey(event) {
        const k = event.key
        if (k === Qt.Key_Escape) {
            cancel()
        } else if (k === Qt.Key_Up) {
            if (!onSubmit) {
                cursor = Math.max(0, cursor - 1)
                if (editing) {
                    editing = false
                    panel.forceActiveFocus()
                }
            }
        } else if (k === Qt.Key_Down) {
            if (!onSubmit) {
                cursor = Math.min(customRow, cursor + 1)
            }
        } else if (k === Qt.Key_Return || k === Qt.Key_Enter) {
            confirm()
        } else if (k === Qt.Key_Space && multi && !editing) {
            toggle(cursor)
        } else if (k === Qt.Key_Tab || k === Qt.Key_Right) {
            moveTab(1)
        } else if (k === Qt.Key_Backtab || k === Qt.Key_Left) {
            moveTab(-1)
        } else if (k >= Qt.Key_1 && k <= Qt.Key_9 && !editing && !onSubmit) {
            const row = k - Qt.Key_1
            if (row <= customRow) {
                cursor = row
                if (multi && row < customRow) {
                    toggle(row)
                } else {
                    choose(row)
                }
            }
        } else {
            return false
        }
        return true
    }

    NineSlice {
        anchors.fill: parent
        sheet: "ui_panel_dark"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: panel.takeFocus()
    }

    Column {
        id: body
        x: 6
        y: 5
        width: panel.width - 12
        spacing: 4

        // Tabs, ask counter, hide.
        Item {
            width: parent.width
            height: 12

            Row {
                spacing: 2
                Repeater {
                    model: panel.several ? panel.qs.length + 1 : panel.qs.length
                    Item {
                        id: tabItem
                        required property int index
                        readonly property bool isSubmit: index === panel.qs.length
                        readonly property string title: {
                            if (isSubmit) {
                                return qsTr("Submit")
                            }
                            const h = panel.qs[index].header || qsTr("Q%1").arg(index + 1)
                            return h.length > 12 ? h.slice(0, 11) + "…" : h
                        }
                        width: tabText.implicitWidth + 12 + (isSubmit ? 0 : 5)
                        height: 12
                        NineSlice {
                            anchors.fill: parent
                            sheet: "ui_tab"
                            frame: panel.tab === tabItem.index ? "active" : "inactive"
                        }
                        Sprite {
                            visible: !tabItem.isSubmit
                            sheet: "ui_led_dot"
                            frame: panel.answered[tabItem.index] ? "ok" : "off"
                            px: 4
                            py: 5
                        }
                        PixelText {
                            id: tabText
                            x: tabItem.isSubmit ? 6 : 9
                            y: 3
                            text: tabItem.title
                            color: panel.tab === tabItem.index ? OfficeArt.text : OfficeArt.dim
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                panel.tab = tabItem.index
                                panel.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            Row {
                anchors.right: parent.right
                y: 1
                spacing: 3
                PixelText {
                    visible: panel.asks.length > 1
                    y: 2
                    text: (Math.min(panel.askIndex, panel.asks.length - 1) + 1) + "/" + panel.asks.length
                    color: OfficeArt.accent
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: panel.askIndex = (panel.askIndex + 1) % panel.asks.length
                    }
                }
                Item {
                    width: 9
                    height: 9
                    Sprite {
                        sheet: "icons"
                        frame: "close"
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -2
                        cursorShape: Qt.PointingHandCursor
                        onClicked: panel.view.questionsHidden = true
                    }
                }
            }
        }

        // The question.
        Item {
            visible: !panel.onSubmit
            width: parent.width
            height: questionText.height + 1
            Sprite {
                sheet: "icons"
                frame: "question"
                px: 0
                py: 0
            }
            PixelText {
                id: questionText
                x: 12
                y: 1
                width: parent.width - 12
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                text: panel.q ? panel.q.question : ""
            }
        }

        // Options.
        Column {
            visible: !panel.onSubmit
            width: parent.width
            spacing: 1

            Repeater {
                model: panel.options
                Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool active: panel.cursor === index
                    readonly property bool picked: (panel.picks[panel.tab] ?? []).indexOf(modelData.label) >= 0
                    width: parent.width
                    height: 12

                    NineSlice {
                        visible: row.active
                        x: -2
                        width: parent.width + 4
                        height: 12
                        sheet: "ui_row_highlight"
                    }
                    PixelText {
                        x: 2
                        y: 3
                        text: (row.index + 1) + "."
                        color: row.active ? OfficeArt.accent : OfficeArt.dim
                    }
                    Sprite {
                        visible: panel.multi
                        sheet: "ui_check"
                        frame: row.picked ? "on" : "off"
                        px: 16
                        py: 2
                    }
                    PixelText {
                        id: optLabel
                        x: panel.multi ? 28 : 17
                        y: 3
                        text: row.modelData.label
                        color: row.picked && !panel.multi ? OfficeArt.ok : OfficeArt.text
                    }
                    PixelText {
                        x: optLabel.x + optLabel.implicitWidth + 6
                        y: 3
                        width: Math.max(0, row.width - x - 2)
                        elide: Text.ElideRight
                        text: row.modelData.description ?? ""
                        color: OfficeArt.dim
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: panel.cursor = row.index
                        onClicked: {
                            panel.cursor = row.index
                            panel.forceActiveFocus()
                            if (panel.multi) {
                                panel.toggle(row.index)
                            } else {
                                panel.choose(row.index)
                            }
                        }
                    }
                }
            }

            // "Type something." - becomes a text field.
            Item {
                id: customItem
                readonly property bool active: panel.cursor === panel.customRow
                width: parent.width
                height: 15
                PixelText {
                    x: 2
                    y: 4
                    text: (panel.customRow + 1) + "."
                    color: customItem.active ? OfficeArt.accent : OfficeArt.dim
                }
                NineSlice {
                    x: 15
                    width: parent.width - 15
                    height: 14
                    sheet: "ui_textfield"
                    frame: panel.editing ? "focus" : "normal"
                }
                PixelText {
                    x: 20
                    y: 4
                    visible: field.text.length === 0
                    text: qsTr("Type something.")
                    color: customItem.active ? OfficeArt.text : OfficeArt.dim
                }
                TextInput {
                    id: field
                    x: 20
                    y: 2
                    width: parent.width - 26
                    height: 11
                    clip: true
                    font.family: "monogram"
                    font.pixelSize: 16
                    font.hintingPreference: Font.PreferFullHinting
                    renderType: Text.NativeRendering
                    color: OfficeArt.text
                    selectionColor: "#5b4a6e"
                    selectedTextColor: OfficeArt.text
                    activeFocusOnTab: false
                    cursorDelegate: Sprite {
                        sheet: "ui_caret"
                        animation: field.activeFocus ? "blink" : ""
                        frame: "off"
                        y: 1
                    }
                    onActiveFocusChanged: if (activeFocus) {
                        panel.editing = true
                        panel.cursor = panel.customRow
                    }
                    onTextChanged: {
                        if (panel.tab >= panel.customs.length) {
                            return
                        }
                        const c = panel.customs.slice()
                        c[panel.tab] = text
                        panel.customs = c
                    }
                    onAccepted: panel.acceptCustom()
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab
                                || event.key === Qt.Key_Up || event.key === Qt.Key_Escape) {
                            event.accepted = panel.handleKey(event)
                        }
                    }
                    Connections {
                        target: panel
                        function onTabChanged() {
                            field.text = panel.customs[panel.tab] ?? ""
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    visible: !panel.editing
                    cursorShape: Qt.IBeamCursor
                    hoverEnabled: true
                    onEntered: panel.cursor = panel.customRow
                    onClicked: panel.choose(panel.customRow)
                }
            }
        }

        // Submit tab: the answers so far.
        Column {
            visible: panel.onSubmit
            width: parent.width
            spacing: 2
            PixelText {
                text: qsTr("Review your answers")
                color: OfficeArt.dim
            }
            Repeater {
                model: panel.onSubmit ? panel.qs : []
                Item {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 10
                    PixelText {
                        id: hdr
                        x: 2
                        text: (modelData.header || qsTr("Q%1").arg(index + 1)) + ":"
                        color: OfficeArt.dim
                    }
                    PixelText {
                        x: hdr.x + hdr.implicitWidth + 5
                        width: parent.width - x
                        elide: Text.ElideRight
                        readonly property string answer: panel.answerText(index)
                        text: answer.length > 0 ? answer : qsTr("not answered")
                        color: answer.length > 0 ? OfficeArt.ok : OfficeArt.warn
                    }
                }
            }
        }

        // Footer.
        Item {
            width: parent.width
            height: 15
            PixelText {
                y: 4
                width: parent.width - buttons.width - 4
                elide: Text.ElideRight
                text: panel.current ? panel.current.agentName + (panel.current.project ? " · " + panel.current.project : "") : ""
                color: OfficeArt.dim
            }
            Row {
                id: buttons
                anchors.right: parent.right
                spacing: 3
                PixelButton {
                    text: qsTr("In terminal")
                    icon: "terminal"
                    onClicked: panel.cancel()
                }
                PixelButton {
                    accent: true
                    text: panel.onSubmit || !panel.several ? qsTr("Answer") : qsTr("Next")
                    onClicked: {
                        panel.forceActiveFocus()
                        if (panel.editing) {
                            panel.acceptCustom()
                        } else {
                            panel.confirm()
                        }
                    }
                }
            }
        }
    }
}
