// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The island itself: a body whose size is dictated by whichever view currently
 * has the floor, plus a satellite that buds off it when something is playing.
 *
 * There is no background here. The bodies are drawn behind by the stage's goo
 * layers, so that two shapes can visually fuse; this item only carries content,
 * geometry and interaction.
 */
Item {
    id: island

    /** The surface this island lives on, needed to ask for the keyboard. */
    property var targetWindow: null

    // ---- what the stage needs to draw ------------------------------------
    readonly property rect mainGeometry: Qt.rect(x, y, width, height)
    readonly property rect satelliteGeometry: Qt.rect(x + width + satelliteGap,
                                                      y + (height - satelliteSize) / 2,
                                                      satelliteSize,
                                                      satelliteSize)
    /**
     * How round the corners are, worked out from what the island is currently
     * holding rather than from which view is on it.
     *
     * The lozenge - half the height, the shape everybody recognises - is right
     * for an island holding one row: a pill, an OSD, a line of text. It is
     * wrong the moment there are rows stacked above each other, and wrong in a
     * way that is easy to miss, because the corner then curves through the
     * outermost row rather than around it. A 160px panel with an 80px corner
     * has its bottom row of buttons hanging in mid air.
     *
     * So the test is the number of rows, not the aspect ratio: past one row
     * the corner is a card's, proportional to the height and capped, and
     * nothing is ever rounder than half the shorter side.
     */
    readonly property real cornerRadius: {
        const ceiling = Math.min(width, height) / 2
        const configured = Cfg.island.cornerRadius ?? 0
        if (configured > 0) {
            return Math.min(configured, ceiling)
        }
        if (height <= Cfg.collapsedHeight * 1.5) {
            return ceiling
        }
        return Math.min(ceiling, Math.max(20, Math.min(height * 0.3, 32)))
    }

    /**
     * How far the corner curve reaches in at the height where content actually
     * sits - a dozen pixels from the edge, which is where the outermost row of
     * a view ends up - rather than at the 45 degrees where the curve is easiest
     * to reason about and least relevant.
     *
     * The difference is not academic: on an 80px corner the diagonal is 24px in
     * and the row twelve pixels from the edge is 38px in. Views keep at least
     * this much clear, which is what makes "inside the black" a property of the
     * layout instead of something to be checked by eye afterwards.
     */
    readonly property real edgeInset: {
        const r = cornerRadius
        const row = 12
        if (r <= row) {
            return 0
        }
        return Math.ceil(r - Math.sqrt(r * r - (r - row) * (r - row)))
    }

    /**
     * A notch grows out of the screen edge: the two corners touching the edge
     * are square, so there is no seam between the island and the bezel. A pill
     * is rounded all the way around and floats below the edge instead.
     */
    readonly property bool notch: Cfg.shape === "notch"
    readonly property real topRadius: notch ? (Cfg.atBottom ? cornerRadius : 0) : cornerRadius
    readonly property real bottomRadius: notch ? (Cfg.atBottom ? 0 : cornerRadius) : cornerRadius

    // ---- state -----------------------------------------------------------
    property bool expanded: false
    property bool notificationSticky: false
    property var currentNotification: ({})

    /**
     * The assistant has the island to itself while it is in front. It is not
     * one more thing competing for the pill: it was opened deliberately, by a
     * long press, and anything that interrupted it would interrupt the user
     * mid-sentence.
     */
    readonly property bool aiEnabled: Cfg.aiEnabled && !locked
    readonly property bool aiActive: aiEnabled && App.ai.engaged && !App.ai.background
    /** Still working, but out of the way: only the pill reports it. */
    readonly property bool aiInBackground: aiEnabled && App.ai.background && App.ai.busy

    readonly property bool hasMedia: App.media.active !== null && (Cfg.modules.media ?? true)
    /** A drag hovering the island, or a transfer in either direction. */
    readonly property bool sharingEnabled: Cfg.modules.sharing ?? true
    readonly property bool dragActive: dropZone.containsDrag
    readonly property bool sharing: sharingEnabled && (dragActive || App.share.state !== "idle")
    readonly property bool mediaPlaying: hasMedia && App.media.active.playing
    readonly property bool hovered: hoverHandler.hovered

    // ---- lock screen -----------------------------------------------------
    //
    // The island can be allowed to outlive the lock screen, which makes what it
    // shows there a privacy question rather than a layout one: a notification
    // body or a backlog of them is exactly what a locked machine should not be
    // handing out. What is on by default there is what a passer-by may read.
    readonly property bool locked: App.lock.locked
    readonly property bool lockedQuiet: locked && !(Cfg.lockScreen.showNotifications ?? false)
    readonly property bool lockedNoMedia: locked && !(Cfg.lockScreen.showMedia ?? true)
    readonly property bool canExpand: !locked || (Cfg.lockScreen.allowExpanding ?? false)

    /**
     * Priority order, highest first. An OSD outranks a notification because it
     * is a direct response to something the user just did.
     */
    readonly property string mode: {
        // The assistant outranks everything, including sharing: the user
        // opened it on purpose and is very likely typing into it.
        if (aiActive) {
            return "ai"
        }
        // Sharing is next: it is either a drag the user is holding over the
        // island right now, or a stranger waiting for an answer.
        if (sharing) {
            return "share"
        }
        if (expanded) {
            return "expanded"
        }
        if (!App.busTapActive && busWarning.running) {
            return "warning"
        }
        if (osdTimer.running) {
            return "osd"
        }
        if ((notificationTimer.running || notificationSticky) && !lockedQuiet) {
            return "notification"
        }
        if (mediaTimer.running && !lockedNoMedia) {
            return "media"
        }
        if (hovered && hasMedia && !lockedNoMedia && (Cfg.behavior.hoverPeek ?? true)) {
            return "media"
        }
        return "idle"
    }

    readonly property bool satelliteVisible: mode === "idle" && mediaPlaying && !lockedNoMedia
                                             && (Cfg.modules.media ?? true)
    property real satelliteSize: satelliteVisible ? Cfg.collapsedHeight : 0
    // A negative gap keeps the satellite tucked inside the body until it is
    // ready to emerge, which is what makes the separation read as budding off.
    property real satelliteGap: satelliteVisible ? 9 : -Cfg.collapsedHeight

    width: Math.max(0, Math.min(mode === "ai" ? Cfg.maxAiWidth : Cfg.maxWidth, stack.contentWidth))
    height: Math.max(0, stack.contentHeight)

    Behavior on width {
        SpringAnimation {
            spring: Cfg.spring
            damping: Cfg.damping
            mass: 1.0
            epsilon: 0.25
        }
    }
    Behavior on height {
        SpringAnimation {
            spring: Cfg.spring
            damping: Cfg.damping
            mass: 1.0
            epsilon: 0.25
        }
    }
    Behavior on satelliteSize {
        SpringAnimation {
            spring: Cfg.spring
            damping: 0.5
            epsilon: 0.2
        }
    }
    Behavior on satelliteGap {
        SpringAnimation {
            spring: Cfg.spring
            damping: 0.5
            epsilon: 0.2
        }
    }

    Component.onCompleted: if (App.debugState) {
        console.warn("atoll: island ready; osd=" + App.osd + " notif=" + App.notifications
                     + " media=" + App.media + " ipc=" + App.ipc + " busTap=" + App.busTapActive)
    }

    onModeChanged: if (App.debugState) {
        console.warn("atoll: mode ->", mode, "size", Math.round(width) + "x" + Math.round(height))
    }

    // ---- event plumbing --------------------------------------------------
    Timer {
        id: osdTimer
        interval: Cfg.osdTimeout
    }
    Timer {
        id: notificationTimer
        interval: Cfg.notificationTimeout
    }
    Timer {
        id: mediaTimer
        interval: Cfg.mediaPeekDuration
    }
    Timer {
        id: busWarning
        interval: 9000
    }
    Timer {
        id: leaveTimer
        interval: 420
        onTriggered: {
            if (island.expanded && !island.hovered && (Cfg.behavior.collapseOnLeave ?? true)) {
                island.expanded = false
            }
        }
    }

    onHoveredChanged: {
        if (hovered) {
            leaveTimer.stop()
            // Touching the island acknowledges whatever it was showing.
            notificationSticky = false
        } else if (expanded) {
            leaveTimer.restart()
        }
    }

    Connections {
        target: App.osd

        function onTriggered() {
            if (island.expanded) {
                return // The dashboard already shows the same information.
            }
            osdTimer.restart()
        }

        function onDismissed() {
            osdTimer.stop()
        }
    }

    Connections {
        target: App.notifications

        function onArrived(notification) {
            if (App.debugState) {
                console.warn("atoll: notification from", notification.appName, "-", notification.summary)
            }
            if (island.expanded || (notification.transient ?? false)) {
                return
            }
            island.currentNotification = notification
            island.notificationSticky = (notification.urgency ?? 1) >= 2
                    && (Cfg.notifications.criticalStaysOpen ?? true)
            notificationTimer.restart()
        }
    }

    Connections {
        target: App.media

        function onTrackChanged() {
            if (!(Cfg.media.showOnPlay ?? true) || island.expanded || !island.mediaPlaying) {
                return
            }
            mediaTimer.restart()
        }
    }

    Connections {
        target: App.ipc

        function onExpandRequested() {
            if (App.debugState) {
                console.warn("atoll: ipc expand")
            }
            island.expanded = island.canExpand
        }
        function onCollapseRequested() {
            island.expanded = false
        }
        function onToggleRequested() {
            island.expanded = !island.expanded
        }
        function onDismissRequested() {
            if (island.aiActive) {
                App.ai.dismiss()
            }
            island.expanded = false
            island.notificationSticky = false
            notificationTimer.stop()
        }
    }

    Connections {
        target: App.lock

        function onLockedChanged() {
            // Whatever was on screen belongs to the session that just went away.
            if (!island.canExpand) {
                island.expanded = false
            }
            if (island.lockedQuiet) {
                island.notificationSticky = false
                notificationTimer.stop()
            }
        }
    }

    Connections {
        target: App

        function onBusTapChanged() {
            if (!App.busTapActive) {
                busWarning.restart()
            }
        }
    }

    // Views are loaded rather than nested, so they cannot see the island they
    // are sitting in. The inset the corners demand is published where they can
    // read it, the same way the accent is.
    Binding {
        target: Theme
        property: "edgeInset"
        value: island.edgeInset
        restoreMode: Binding.RestoreNone
    }

    // The accent follows the current cover art unless the user pinned a colour.
    Binding {
        target: Theme
        property: "dynamicAccent"
        value: App.media.active && App.media.active.artUrl.length > 0 && App.media.active.accent.a > 0
               ? App.media.active.accent
               : Theme.accentFallback
        restoreMode: Binding.RestoreNone
    }

    // ---- content ---------------------------------------------------------
    Item {
        id: stack

        // The island tracks whichever loader is currently in front.
        property bool frontIsA: true
        property Item frontItem: frontIsA ? loaderA.item : loaderB.item
        property real contentWidth: frontItem ? Math.max(frontItem.implicitWidth, 0) : Cfg.collapsedWidth
        property real contentHeight: frontItem ? Math.max(frontItem.implicitHeight, 0) : Cfg.collapsedHeight

        property Component target: {
            switch (island.mode) {
            case "ai":
                return aiComponent
            case "share":
                return shareComponent
            case "expanded":
                return expandedComponent
            case "warning":
                return warningComponent
            case "osd":
                return osdComponent
            case "notification":
                return notificationComponent
            case "media":
                return mediaComponent
            default:
                return idleComponent
            }
        }

        anchors.fill: parent
        clip: true

        onTargetChanged: {
            // Ping-pong the two loaders so the outgoing view can fade out
            // while the incoming one fades in.
            if (frontIsA) {
                loaderB.sourceComponent = target
            } else {
                loaderA.sourceComponent = target
            }
            frontIsA = !frontIsA
        }

        Component.onCompleted: loaderA.sourceComponent = target

        Loader {
            id: loaderA
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            opacity: stack.frontIsA ? 1 : 0
            visible: opacity > 0
            scale: stack.frontIsA ? 1 : 0.94

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.fast
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }

        Loader {
            id: loaderB
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            opacity: stack.frontIsA ? 0 : 1
            visible: opacity > 0
            scale: stack.frontIsA ? 0.94 : 1

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.fast
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    // Optional hairline, drawn on top of the goo so it traces the real shape.
    Rectangle {
        anchors.fill: parent
        visible: Theme.border
        radius: island.cornerRadius
        topLeftRadius: island.topRadius
        topRightRadius: island.topRadius
        bottomLeftRadius: island.bottomRadius
        bottomRightRadius: island.bottomRadius
        color: "transparent"
        border.width: 1
        border.color: Theme.borderColor
        antialiasing: true
    }

    // ---- satellite -------------------------------------------------------
    Item {
        x: island.width + island.satelliteGap
        y: (island.height - island.satelliteSize) / 2
        width: island.satelliteSize
        height: island.satelliteSize
        visible: island.satelliteSize > 4
        opacity: island.satelliteVisible ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.fast
            }
        }

        Spectrum {
            anchors.centerIn: parent
            height: parent.height * 0.42
            barWidth: 2
            spacing: 2
            limit: 4
        }
    }

    // ---- interaction -----------------------------------------------------
    //
    // A layer surface gets no keyboard unless it asks, and asking for one
    // permanently would swallow every shortcut in the session. So the island
    // takes the keyboard only while there is a text field on it, and gives it
    // straight back afterwards.
    onAiActiveChanged: if (targetWindow) {
        App.shell.setKeyboardFocus(targetWindow, aiActive)
    }

    Component.onDestruction: if (targetWindow) {
        App.shell.setKeyboardFocus(targetWindow, false)
    }

    HoverHandler {
        id: hoverHandler
    }

    // Files dragged onto the island are offered to whoever is nearby. The
    // island itself is the drop target, so it has to be the thing that grows
    // under the pointer - which is what turning the mode into "share" does.
    DropArea {
        id: dropZone
        anchors.fill: parent
        enabled: island.sharingEnabled
        keys: ["text/uri-list"]

        onEntered: drag => {
            if (!drag.hasUrls) {
                drag.accepted = false
                return
            }
            App.share.probe()
        }

        onDropped: drop => {
            if (!drop.hasUrls) {
                return
            }
            App.share.offer(drop.urls)
            drop.accept(Qt.CopyAction)
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        enabled: island.aiEnabled
        longPressThreshold: Cfg.aiLongPress / 1000
        onLongPressed: {
            App.ai.engage()
            island.expanded = false
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            if (island.aiInBackground) {
                // Whatever it is doing, the user wants to see it again.
                App.ai.bringToFront()
                return
            }
            if (island.mode === "ai") {
                return // The assistant has its own buttons.
            }
            if (island.mode === "share") {
                return // The share view has its own buttons.
            }
            if (island.mode === "notification") {
                island.notificationSticky = false
                notificationTimer.stop()
                return
            }
            if ((Cfg.behavior.clickAction ?? "expand") === "expand" && island.canExpand) {
                island.expanded = !island.expanded
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.MiddleButton
        onTapped: {
            if ((Cfg.behavior.middleClickAction ?? "playPause") === "playPause" && island.hasMedia) {
                App.media.active.playPause()
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: {
            if (island.mode === "ai") {
                App.ai.dismiss()
                return
            }
            if (island.mode === "share") {
                App.share.dismiss()
                return
            }
            switch (Cfg.behavior.rightClickAction ?? "settings") {
            case "settings":
                App.openSettings()
                island.expanded = false
                break
            case "collapse":
                island.expanded = false
                break
            }
        }
    }

    WheelHandler {
        enabled: (Cfg.behavior.scrollAdjustsVolume ?? true) && island.mode !== "ai"
                 && island.mode !== "expanded"
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const step = Cfg.behavior.volumeStep ?? 5
            App.adjustVolume(event.angleDelta.y > 0 ? step : -step)
        }
    }

    // ---- view components -------------------------------------------------
    Component {
        id: idleComponent
        IdleView {}
    }

    Component {
        id: osdComponent
        OsdView {}
    }

    Component {
        id: mediaComponent
        MediaView {
            interactive: island.hovered
        }
    }

    Component {
        id: notificationComponent
        NotificationView {
            notification: island.currentNotification
            onDismissRequested: {
                island.notificationSticky = false
                notificationTimer.stop()
            }
        }
    }

    Component {
        id: expandedComponent
        ExpandedView {
            onCollapseRequested: island.expanded = false
        }
    }

    Component {
        id: shareComponent
        ShareView {
            dragging: island.dragActive
        }
    }

    Component {
        id: aiComponent
        AiView {
            onDismissRequested: App.ai.dismiss()
        }
    }

    Component {
        id: warningComponent
        CallToActionView {}
    }
}
