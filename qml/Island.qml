// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Shapes
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
    property bool focusExpanded: false
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

    /** The Claude Code office (docs/office-v2.md): right below the assistant. */
    readonly property bool officeActive: App.office.open && !locked

    readonly property bool hasMedia: App.media.active !== null && (Cfg.modules.media ?? true)
    /** A drag hovering the island, or a transfer in either direction. */
    readonly property bool sharingEnabled: Cfg.modules.sharing ?? true
    readonly property bool dragActive: dropZone.containsDrag
    readonly property bool sharing: sharingEnabled && (dragActive || App.share.state !== "idle")
    readonly property bool mediaPlaying: hasMedia && App.media.active.playing
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool cleanSurface: mode === "focusDetail" || (mode === "media" && hovered) || mode === "office"

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
    /** Views with buttons of their own, which the pointer triggers stay out of. */
    readonly property bool ownButtons: mode === "ai" || mode === "office" || mode === "share" || mode === "focusDetail"

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
        if (officeActive) {
            return "office"
        }
        // Sharing is next: it is either a drag the user is holding over the
        // island right now, or a stranger waiting for an answer.
        if (sharing) {
            return "share"
        }
        if (focusExpanded && !locked) return "focusDetail"
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
        if (hovered && hasMedia && !lockedNoMedia && Cfg.control("hover") === "peek") {
            return "media"
        }
        if (App.focus.state.active) return "focus"
        return "idle"
    }

    readonly property bool satelliteVisible: mode === "idle" && mediaPlaying && !lockedNoMedia
                                             && (Cfg.modules.media ?? true)
    property real satelliteSize: satelliteVisible ? Cfg.collapsedHeight : 0
    // A negative gap keeps the satellite tucked inside the body until it is
    // ready to emerge, which is what makes the separation read as budding off.
    property real satelliteGap: satelliteVisible ? 9 : -Cfg.collapsedHeight

    width: Math.max(0, Math.min(mode === "office" ? officeMaxWidth
                                : (mode === "ai" ? Cfg.maxAiWidth : Cfg.maxWidth), stack.contentWidth))
    readonly property real officeMaxWidth: 0.94 * (targetWindow && targetWindow.screen ? targetWindow.screen.width : 1920)
    height: Math.max(0, stack.contentHeight)

    Behavior on width {
        SpringAnimation {
            spring: Cfg.spring
            damping: island.cleanSurface ? 0.86 : Cfg.damping
            mass: 1.0
            epsilon: 0.25
        }
    }
    Behavior on height {
        SpringAnimation {
            spring: Cfg.spring
            damping: island.cleanSurface ? 0.86 : Cfg.damping
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
            if ((island.expanded || island.focusExpanded) && !island.hovered && Cfg.collapseOnLeave
                    && !(island.targetWindow && island.targetWindow.activeFocusItem
                         && island.targetWindow.activeFocusItem.objectName === "focusThought")) {
                island.expanded = false
                island.focusExpanded = false
            }
        }
    }

    // Resting the pointer runs the hover action once; "peek" is not an action
    // but a mode of its own, worked out in `mode` above.
    Timer {
        id: hoverTimer
        interval: Cfg.hoverDelay
        onTriggered: {
            const action = Cfg.control("hover")
            if (!island.hovered || island.ownButtons) {
                return
            }
            if (action === "expand") {
                island.expanded = island.expanded || island.canExpand
                return
            }
            island.runAction(action, "hover")
        }
    }

    onHoveredChanged: {
        const hoverAction = Cfg.control("hover")
        if (hovered && hoverAction !== "none" && hoverAction !== "peek") {
            hoverTimer.restart()
        } else {
            hoverTimer.stop()
        }
        if (hovered) {
            leaveTimer.stop()
            // Touching the island acknowledges whatever it was showing.
            notificationSticky = false
        } else if (expanded || focusExpanded) {
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
            if (App.focus.state.quiet && !((notification.urgency ?? 1) >= 2 && Cfg.get("focus.allowCritical", true))) return
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
            island.focusExpanded = false
            if (App.debugState) {
                console.warn("atoll: ipc expand")
            }
            island.expanded = island.canExpand
        }
        function onCollapseRequested() {
            island.focusExpanded = false
            island.expanded = false
        }
        function onToggleRequested() {
            if (island.focusExpanded) { island.focusExpanded = false; island.expanded = false; return }
            island.expanded = !island.expanded
        }
        function onActionRequested(id) {
            island.runAction(id, "ipc")
        }
        function onDismissRequested() {
            island.focusExpanded = false
            if (island.aiActive) {
                App.ai.dismiss()
            }
            island.expanded = false
            island.notificationSticky = false
            notificationTimer.stop()
        }
    }

    Connections {
        target: App.shortcuts

        function onActivated(id) {
            island.runAction(id, "shortcut")
        }
    }

    Connections {
        target: App.lock

        function onLockedChanged() {
            if (island.locked) island.focusExpanded = false
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
            case "focusDetail":
                return focusDetailComponent
            case "focus":
                return focusComponent
            case "ai":
                return aiComponent
            case "office":
                return officeComponent
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
                return island.hovered ? mediaHoverComponent : mediaComponent
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

    // Concave shoulders let the hover panel grow out of the screen edge.
    Shape {
        x: -20
        y: Cfg.atBottom ? island.height - 20 : 0
        width: island.width + 40
        height: 20
        rotation: Cfg.atBottom ? 180 : 0
        opacity: island.notch && island.cleanSurface ? 1 : 0
        visible: opacity > 0
        antialiasing: true
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        ShapePath {
            strokeWidth: -1
            fillColor: "#000000"
            startX: 0; startY: 0
            PathLine { x: 20; y: 0 }
            PathLine { x: 20; y: 20 }
            PathCubic { x: 0; y: 0; control1X: 20; control1Y: 6; control2X: 14; control2Y: 0 }
        }
        ShapePath {
            strokeWidth: -1
            fillColor: "#000000"
            startX: island.width + 40; startY: 0
            PathLine { x: island.width + 20; y: 0 }
            PathLine { x: island.width + 20; y: 20 }
            PathCubic { x: island.width + 40; y: 0; control1X: island.width + 20; control1Y: 6; control2X: island.width + 26; control2Y: 0 }
        }
    }

    // Optional hairline, drawn on top of the goo so it traces the real shape.
    Rectangle {
        anchors.fill: parent
        visible: Theme.border && !island.cleanSurface
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
        App.shell.setKeyboardFocus(targetWindow, aiActive || officeActive)
    }
    onOfficeActiveChanged: if (targetWindow) {
        App.shell.setKeyboardFocus(targetWindow, aiActive || officeActive)
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

    /**
     * Run one action from Actions.qml. Every trigger ends up here - pointer,
     * global shortcut and `atollctl action` - so the mode guards stay with
     * the handlers and this only does what the action says.
     */
    function runAction(id, source) {
        if (App.debugState) {
            console.warn("atoll: action", id, "from", source)
        }
        switch (id) {
        case "none":
        case "peek":
            return
        case "expand":
            if (focusExpanded) {
                focusExpanded = false
                expanded = false
                return
            }
            expanded = !expanded && canExpand
            return
        case "collapse":
            focusExpanded = false
            expanded = false
            return
        case "dismiss":
            focusExpanded = false
            if (aiActive) {
                App.ai.dismiss()
            } else if (officeActive) {
                App.office.hide()
            } else if (mode === "share") {
                App.share.dismiss()
            }
            expanded = false
            notificationSticky = false
            notificationTimer.stop()
            return
        case "settings":
            App.openSettings()
            expanded = false
            return
        case "office":
            App.office.toggle()
            return
        case "officeOrPlayPause":
            if (App.office.active) {
                App.office.toggle()
            } else if (hasMedia) {
                App.media.active.playPause()
            }
            return
        case "assistant":
            if (aiEnabled) {
                App.ai.engage()
                expanded = false // The assistant takes the dashboard's place.
            }
            return
        case "playPause":
            if (hasMedia) App.media.active.playPause()
            return
        case "next":
            if (hasMedia) App.media.active.next()
            return
        case "previous":
            if (hasMedia) App.media.active.previous()
            return
        case "volumeUp":
            App.adjustVolume(Cfg.volumeStep)
            return
        case "volumeDown":
            App.adjustVolume(-Cfg.volumeStep)
            return
        case "focus":
            if (App.focus.state.active) {
                App.focus.stop()
            } else {
                App.focus.start()
            }
            return
        case "dnd":
            App.notifications.doNotDisturb = !App.notifications.doNotDisturb
            return
        default:
            console.warn("atoll: unknown action", id)
        }
    }

    /** Actions that leave the island's layout alone, allowed over its own buttons. */
    function passive(id) {
        return ["office", "officeOrPlayPause", "playPause", "next", "previous",
                "volumeUp", "volumeDown", "dnd", "none"].includes(id)
    }

    TapHandler {
        id: leftTap
        acceptedButtons: Qt.LeftButton
        // With nothing on the long press, a held click is still not a click
        // after the usual hold interval.
        longPressThreshold: Cfg.control("longPress") === "none"
                            ? Qt.styleHints.mousePressAndHoldInterval / 1000
                            : Cfg.longPressMs / 1000

        onLongPressed: {
            clickTimer.stop()
            const action = Cfg.control("longPress")
            if (island.ownButtons && action !== "assistant") {
                return
            }
            island.runAction(action, "longPress")
        }

        /** True when the tap went to whatever the island is showing. */
        function claimed() {
            if (island.aiInBackground) {
                // Whatever it is doing, the user wants to see it again.
                App.ai.bringToFront()
                return true
            }
            if (island.ownButtons) {
                return true // Those views have their own buttons.
            }
            if (island.mode === "notification") {
                island.notificationSticky = false
                notificationTimer.stop()
                return true
            }
            return false
        }

        onTapped: {
            if (claimed()) {
                clickTimer.stop()
                return
            }
            // A single click only waits for a second one when the second one
            // means something; otherwise it acts on release.
            if (Cfg.control("doubleClick") === "none") {
                island.runAction(Cfg.control("click"), "click")
            } else if (clickTimer.running) {
                clickTimer.stop()
                island.runAction(Cfg.control("doubleClick"), "doubleClick")
            } else {
                clickTimer.restart()
            }
        }
    }

    Timer {
        id: clickTimer
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: if (!island.ownButtons) {
            island.runAction(Cfg.control("click"), "click")
        }
    }

    TapHandler {
        acceptedButtons: Qt.MiddleButton

        // Whether this press/release pair was routed to the office rather
        // than to the configured action, decided fresh each press.
        property bool officeHandled: false
        // When the office was opened by this press, so release can tell a
        // quick toggle-click from a long peek-hold.
        property real pressedAt: 0

        function officeAction() {
            const action = Cfg.control("middleClick")
            return action === "office" || (action === "officeOrPlayPause" && App.office.active)
        }

        onPressedChanged: {
            if (!pressed) {
                if (officeHandled && pressedAt > 0 && Date.now() - pressedAt > 350) {
                    App.office.hide() // held past the peek threshold
                }
                officeHandled = false
                pressedAt = 0
                return
            }
            officeHandled = officeAction()
            if (!officeHandled) {
                return
            }
            if (App.office.open) {
                App.office.hide() // pressing again while it is already open
            } else {
                App.office.show()
                pressedAt = Date.now()
            }
        }

        onTapped: {
            if (officeAction()) {
                return // already handled on press/release above
            }
            const action = Cfg.control("middleClick")
            if (island.ownButtons && !island.passive(action)) {
                return
            }
            island.runAction(action, "middleClick")
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
            if (island.mode === "office") {
                return
            }
            island.runAction(Cfg.control("rightClick"), "rightClick")
        }
    }

    WheelHandler {
        // Anything but a volume step fires once per flick of the wheel: a
        // gesture ends after this long without another wheel event.
        readonly property int gestureGap: 400
        property real lastEvent: 0
        property string lastAction: ""

        enabled: (Cfg.control("scrollUp") !== "none" || Cfg.control("scrollDown") !== "none")
                 && island.mode !== "ai" && island.mode !== "office"
                 && island.mode !== "expanded" && island.mode !== "focusDetail"
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            if (event.angleDelta.y === 0) {
                return
            }
            const action = Cfg.control(event.angleDelta.y > 0 ? "scrollUp" : "scrollDown")
            if (action === "volumeUp" || action === "volumeDown") {
                island.runAction(action, "scroll")
                return
            }
            const now = Date.now()
            const fresh = now - lastEvent > gestureGap || action !== lastAction
            lastEvent = now
            lastAction = action
            if (fresh) {
                island.runAction(action, "scroll")
            }
        }
    }

    // ---- view components -------------------------------------------------
    Component {
        id: focusDetailComponent
        FocusView {
            keyboardWindow: island.targetWindow
            onBackRequested: { island.expanded = true; island.focusExpanded = false }
        }
    }
    Component {
        id: focusComponent
        FocusView { compact: true }
    }

    Component {
        id: idleComponent
        IdleView {}
    }

    Component {
        id: osdComponent
        OsdView {}
    }

    Component {
        id: mediaHoverComponent
        MediaHoverView {}
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
            onFocusRequested: { island.focusExpanded = true; island.expanded = false }
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

    Component {
        id: officeComponent
        OfficeView {}
    }
}
