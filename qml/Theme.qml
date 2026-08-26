// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * Visual tokens. Colours come from the config, except the accent, which by
 * default follows whatever is playing: the island picks up the dominant colour
 * of the current album art and falls back to a fixed blue when nothing is on.
 */
QtObject {
    id: theme

    readonly property color background: Qt.rgba(
        Qt.color(Cfg.appearance.background ?? "#0b0b0e").r,
        Qt.color(Cfg.appearance.background ?? "#0b0b0e").g,
        Qt.color(Cfg.appearance.background ?? "#0b0b0e").b,
        Cfg.appearance.backgroundOpacity ?? 0.97)

    readonly property color foreground: Cfg.appearance.foreground ?? "#f4f4f7"
    readonly property color muted: Cfg.appearance.muted ?? "#9a9aa6"
    readonly property color borderColor: Cfg.appearance.borderColor ?? "#1affffff"
    readonly property bool border: Cfg.appearance.border ?? true
    readonly property bool shadow: Cfg.appearance.shadow ?? true
    readonly property real shadowOpacity: Cfg.appearance.shadowOpacity ?? 0.45

    readonly property color accentFallback: Cfg.appearance.accentFallback ?? "#5aa2ff"
    property color dynamicAccent: accentFallback

    /**
     * How far in from the sides content has to stay to remain over the black.
     * Written by the island, because only it knows how round it currently is.
     */
    property real edgeInset: 0

    readonly property color accent: {
        const configured = Cfg.appearance.accent ?? "auto"
        return configured === "auto" ? dynamicAccent : Qt.color(configured)
    }

    /**
     * The assistant's own colour, which is deliberately not the accent.
     *
     * The accent follows the album art, and an assistant panel that changes
     * colour with the music changes colour under somebody who is halfway
     * through reading an answer in it. The assistant is the one thing on the
     * island that is not about what is playing, so it is white and stays
     * white.
     */
    readonly property color assistantTint: Cfg.appearance.assistantTint ?? "#ffffff"

    readonly property color critical: "#ff5f57"
    readonly property color positive: "#32d74b"

    /**
     * A wash of the foreground at some alpha. Cards, hovers and scroll bars
     * are painted with this instead of fixed white, so they stay visible over
     * whatever background is configured - white washes on a dark island,
     * dark ones on a light.
     */
    function tint(alpha) {
        return Qt.rgba(foreground.r, foreground.g, foreground.b, alpha)
    }

    readonly property string fontFamily: (Cfg.appearance.fontFamily ?? "") || Qt.application.font.family
    readonly property real fontScale: Cfg.appearance.fontScale ?? 1.0

    function size(points) {
        return Math.max(6, Math.round(points * fontScale))
    }

    // A soft, slightly overshooting curve: Apple-ish without being bouncy.
    readonly property list<real> morphCurve: [0.32, 0.9, 0.24, 1.0, 1.0, 1.0]
    readonly property int fast: Cfg.ms(140)
    readonly property int normal: Cfg.ms(260)
    readonly property int slow: Cfg.ms(420)
}
