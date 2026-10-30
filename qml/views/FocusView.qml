// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import Atoll

Item {
    id: root
    property bool compact: false
    property bool thoughtsOpen: false
    property var keyboardWindow: null
    signal backRequested()
    readonly property var session: App.focus.state
    readonly property color accent: Cfg.get("focus.accent", "#9ae8cb")
    readonly property bool moving: visible && session.active && !session.paused && Cfg.get("focus.animations", true)
    readonly property string clockText: Math.floor(session.remaining / 60).toString().padStart(2, "0") + ":" + (session.remaining % 60).toString().padStart(2, "0")
    implicitWidth: compact ? Math.max(Cfg.fitCollapsedContent ? Cfg.collapsedHeight : Cfg.collapsedWidth,
                                      compactContent.implicitWidth + Math.max(Cfg.collapsedPadding * 2, Theme.edgeInset * 2))
                           : Math.min(Cfg.maxWidth, 360)
    implicitHeight: compact ? Cfg.collapsedHeight : content.implicitHeight + 36
    onThoughtsOpenChanged: if (!thoughtsOpen) thought.focus = false
    onVisibleChanged: if (!visible) thought.focus = false
    Component.onDestruction: if (keyboardWindow && !App.ai.engaged) App.shell.setKeyboardFocus(keyboardWindow, false)

    Row {
        id: compactContent
        visible: root.compact; anchors.centerIn: parent; spacing: 12
        Rectangle {
            width: 6; height: 6; radius: 3; color: root.accent; anchors.verticalCenter: parent.verticalCenter
            SequentialAnimation on opacity {
                running: root.moving && root.compact; loops: Animation.Infinite
                NumberAnimation { to: .4; duration: Cfg.get("focus.breathSeconds", 6) * 500; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1; duration: Cfg.get("focus.breathSeconds", 6) * 500; easing.type: Easing.InOutSine }
            }
        }
        Text { text: root.session.paused ? "Paused" : root.session.phase === "break" ? "Break" : "Focus"; color: "#f5f5f7"; font.pixelSize: 12 }
        Text { text: Cfg.get("focus.showCountdown", true) ? root.clockText : "On"; color: root.accent; font.pixelSize: 12 }
    }
    Column {
        id: content
        visible: !root.compact
        x: 22; y: 16; width: parent.width - 44; spacing: 14
        Row {
            width: parent.width; spacing: 8
            PillButton { label: "‹"; implicitWidth: 28; implicitHeight: 26; fill: "transparent"; onClicked: root.backRequested() }
            Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 116; text: "Focus"; color: "#f5f5f7"; font.pixelSize: 13; font.weight: Font.DemiBold }
            PillButton { label: "Settings"; implicitHeight: 26; fill: "transparent"; onClicked: App.openSettings("focus") }
        }
        Text {
            width: parent.width
            text: Cfg.get("focus.intention", "One step at a time")
            color: "#94949c"; font.pixelSize: 12; elide: Text.ElideRight
        }
        Row {
            width: parent.width
            Text {
                width: parent.width - 88
                text: root.session.active ? (Cfg.get("focus.showCountdown", true) ? root.clockText : "In focus") : Cfg.get("focus.minutes", 25) + ":00"
                color: "#f5f5f7"; font.pixelSize: 40; font.weight: Font.Light
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: !root.session.active ? "Ready" : root.session.paused ? "Paused" : root.session.phase === "break" ? "On a break" : "Focusing"
                color: root.accent; font.pixelSize: 11
            }
        }
        Rectangle {
            width: parent.width; height: 3; radius: 2; color: "#29292d"
            Rectangle { width: parent.width * root.session.progress; height: 3; radius: 2; color: root.accent
                Behavior on width { NumberAnimation { duration: Cfg.get("focus.animations", true) ? 800 : 0; easing.type: Easing.OutCubic } }
            }
        }
        Flow {
            width: parent.width; spacing: 6
            PillButton { label: !root.session.active ? "Start focus" : root.session.paused ? "Resume" : "Pause"; accented: true; accentColor: "#f5f5f7"; onClicked: root.session.active ? App.focus.togglePause() : App.focus.start() }
            PillButton { visible: root.session.active; label: "+" + Cfg.get("focus.extensionMinutes", 5) + " min"; onClicked: App.focus.extend() }
            PillButton { visible: root.session.active; label: root.session.phase === "break" ? "Focus" : "Break"; onClicked: App.focus.next() }
            PillButton { visible: root.session.active; label: "End"; onClicked: App.focus.stop() }
        }
        Row {
            width: parent.width; spacing: 8
            PillButton { label: root.thoughtsOpen ? "Hide thoughts" : "Park a thought" + (root.session.notes.length ? " · " + root.session.notes.length : ""); implicitHeight: 26; fill: "transparent"; tint: "#94949c"; onClicked: root.thoughtsOpen = !root.thoughtsOpen }
            Text { anchors.verticalCenter: parent.verticalCenter; text: root.session.completed + " completed"; color: "#73737b"; font.pixelSize: 10 }
        }
        Column {
            width: parent.width; visible: root.thoughtsOpen; spacing: 10
            TextField {
                id: thought
                objectName: "focusThought"
                width: parent.width; height: 34
                placeholderText: "Save a thought for later…"
                color: "#f5f5f7"; placeholderTextColor: "#73737b"; font.pixelSize: 11
                background: Rectangle { radius: 10; color: "#1c1c1f"; border.color: thought.activeFocus ? "#54545c" : "transparent" }
                onActiveFocusChanged: if (root.keyboardWindow && !App.ai.engaged) App.shell.setKeyboardFocus(root.keyboardWindow, activeFocus)
                TapHandler { onTapped: { if (root.keyboardWindow) App.shell.setKeyboardFocus(root.keyboardWindow, true); thought.forceActiveFocus() } }
                onAccepted: { App.focus.park(text); text = ""; focus = false }
                Keys.onEscapePressed: focus = false
            }
            Text {
                width: parent.width; visible: root.session.notes.length > 0
                text: root.session.notes.slice(-3).map(t => "· " + t).join("\n")
                color: "#94949c"; font.pixelSize: 11; wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight
            }
            Flow {
                width: parent.width; spacing: 6; visible: root.session.notes.length > 0
                PillButton { label: "Copy all"; onClicked: App.copyText(root.session.notes.join("\n")) }
                PillButton { label: "Clear"; onClicked: App.focus.clearNotes() }
            }
        }
        Text { width: parent.width; text: root.session.error; visible: text.length > 0; color: "#ffc998"; font.pixelSize: 11; wrapMode: Text.Wrap }
    }
}
