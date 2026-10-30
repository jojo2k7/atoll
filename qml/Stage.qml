// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The transparent canvas the island lives on.
 *
 * The layer surface is deliberately much larger than the island so the island
 * can morph without the compositor resizing anything. What keeps the rest of
 * the screen usable is the input region, which is kept clipped to the island's
 * actual bodies and nothing else.
 *
 * The surface is anchored to the top or the bottom edge of the screen by the
 * shell; where the island sits *inside* it is decided here.
 */
Item {
    id: stage

    property var targetWindow: null
    property string screenName: ""

    readonly property bool atBottom: Cfg.atBottom
    readonly property string alignment: Cfg.position.endsWith("left")
                                        ? "left"
                                        : (Cfg.position.endsWith("right") ? "right" : "center")

    /** How far the satellite currently sticks out past the main body. */
    readonly property real satelliteExtent: Math.max(0, island.satelliteGap + island.satelliteSize)
    readonly property real totalWidth: island.width + satelliteExtent

    readonly property real islandX: {
        switch (alignment) {
        case "left":
            return Cfg.sideMargin
        case "right":
            return Math.max(0, stage.width - totalWidth - Cfg.sideMargin)
        default:
            return Math.round((stage.width - totalWidth) / 2)
        }
    }

    GooLayer {
        anchors.fill: parent
        visible: Theme.shadow && opacity > 0.01
        opacity: stage.dodgeActive ? 0.0 : 1.0
        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
        shadowMode: true
        shadowOpacity: Theme.shadowOpacity
        mainGeometry: island.mainGeometry
        satelliteGeometry: island.satelliteGeometry
        mainRadius: island.cornerRadius
        mainTopRadius: island.topRadius
        mainBottomRadius: island.bottomRadius
        satelliteRadius: -1
    }

    GooLayer {
        anchors.fill: parent
        visible: opacity > 0.01
        opacity: stage.dodgeActive ? 0.0 : 1.0
        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
        fill: island.cleanSurface ? "#000000" : Theme.background
        gooey: Cfg.gooey
        // More strength means a wider halo, so bodies fuse from further apart.
        blurRadius: Cfg.collapsedHeight * (0.22 + 0.5 * Cfg.gooeyStrength)
        mainGeometry: island.mainGeometry
        satelliteGeometry: island.satelliteGeometry
        mainRadius: island.cornerRadius
        mainTopRadius: island.topRadius
        mainBottomRadius: island.bottomRadius
        satelliteRadius: -1
    }

    // ---- Dodge / Auto-hide logic ------------------------------------------
    // hideMode: "none" (always visible), "dodge" (dodge app windows), "autohide" (auto-hide when idle)
    readonly property string hideMode: Cfg.get("island.hideMode", "none")
    onHideModeChanged: App.ipc.setDodgeHelper(hideMode === "dodge")
    Component.onCompleted: App.ipc.setDodgeHelper(hideMode === "dodge")

    Timer {
        id: dodgeLinger
        interval: 1200
        repeat: false
    }

    readonly property bool userInteracting: island.hovered || edgeHover.hovered || island.expanded
    onUserInteractingChanged: {
        if (!userInteracting) {
            dodgeLinger.restart()
        } else {
            dodgeLinger.stop()
        }
    }

    readonly property bool dodgeActive: {
        if (island.mode !== "idle" || userInteracting || dodgeLinger.running) {
            return false
        }
        if (hideMode === "autohide") {
            return true
        }
        if (hideMode === "dodge") {
            return App.ipc.windowActive
        }
        return false
    }

    Item {
        id: edgeTrigger
        anchors.top: stage.atBottom ? undefined : parent.top
        anchors.bottom: stage.atBottom ? parent.bottom : undefined
        x: Math.max(0, stage.islandX - 60)
        width: Math.max(280, stage.totalWidth + 120)
        height: 8
        z: 999

        HoverHandler {
            id: edgeHover
        }
    }

    Island {
        id: island
        targetWindow: stage.targetWindow
        x: stage.islandX
        y: {
            if (stage.atBottom) {
                return stage.dodgeActive ? (stage.height + 120) : Math.max(0, stage.height - height)
            } else {
                return stage.dodgeActive ? (-height - 120) : 0
            }
        }
        opacity: stage.dodgeActive ? 0.0 : 1.0

        Behavior on y {
            SpringAnimation {
                spring: Cfg.spring
                damping: Cfg.damping
                mass: 1.0
                epsilon: 0.25
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
    }

    // The dashboard can be taller than the configured canvas, especially with
    // a lyrics panel and a stack of notifications in it. Ask for the room.
    readonly property int neededHeight: Math.ceil(island.height) + 48
    onNeededHeightChanged: if (targetWindow) {
        App.shell.ensureSurfaceHeight(targetWindow, neededHeight)
    }

    // ---- input region ----------------------------------------------------
    //
    // Applied on a timer rather than on every geometry change: each update is
    // a surface commit, and the island moves 60 times a second while morphing.
    QtObject {
        id: mask

        property string applied: ""

        function regions() {
            const rects = []
            if (island.width > 1 && island.height > 1 && island.y > -island.height + 4) {
                rects.push(Qt.rect(island.x, Math.max(0, island.y), island.width, island.height))
            }
            if (island.satelliteSize > 1 && island.y > -island.height + 4) {
                const s = island.satelliteGeometry
                rects.push(Qt.rect(island.x + s.x, Math.max(0, island.y + s.y), s.width, s.height))
            }
            if (stage.hideMode !== "none") {
                // A dodging island comes back when the pointer reaches the edge
                // strip, so that strip has to receive input even while hidden.
                rects.push(Qt.rect(edgeTrigger.x, stage.atBottom ? stage.height - edgeTrigger.height : 0,
                                   edgeTrigger.width, edgeTrigger.height))
            } else if (rects.length === 0 && Cfg.control("hover") !== "none") {
                // A hidden island still needs somewhere to be woken up from.
                const centre = stage.islandX + island.width / 2
                rects.push(Qt.rect(centre - 30, stage.atBottom ? stage.height - 3 : 0, 60, 3))
            }
            return rects
        }

        function apply() {
            if (!stage.targetWindow) {
                return
            }
            const rects = regions()
            const key = rects.map(r => [Math.round(r.x), Math.round(r.y),
                                        Math.round(r.width), Math.round(r.height)].join(":")).join("|")
            if (key === applied) {
                return
            }
            applied = key
            App.shell.setInputRegion(stage.targetWindow, rects)
        }
    }

    Timer {
        interval: 40
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: mask.apply()
    }
}
