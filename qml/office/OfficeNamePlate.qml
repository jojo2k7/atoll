// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The name on the door, drawn in the same 3x5 pixel font the sprite generator
 * uses (scripts/office_art/props.py, FONT3), so it sits on the plate as if it
 * had been painted there. `office.doorName` wins; otherwise the login name.
 * Seven characters fit on the plate: a longer name is cut to its first word,
 * and anything the font has no glyph for is left out.
 */
Item {
    id: plate

    /** Plate interior in scene pixels, relative to this item. */
    property int plateWidth: 28
    property color ink: "#2b2033"

    readonly property var font: ({
        "A": [".#.", "#.#", "###", "#.#", "#.#"],
        "B": ["##.", "#.#", "##.", "#.#", "##."],
        "C": [".##", "#..", "#..", "#..", ".##"],
        "D": ["##.", "#.#", "#.#", "#.#", "##."],
        "E": ["###", "#..", "##.", "#..", "###"],
        "F": ["###", "#..", "##.", "#..", "#.."],
        "G": [".##", "#..", "#.#", "#.#", ".##"],
        "H": ["#.#", "#.#", "###", "#.#", "#.#"],
        "I": ["###", ".#.", ".#.", ".#.", "###"],
        "J": ["..#", "..#", "..#", "#.#", ".#."],
        "K": ["#.#", "#.#", "##.", "#.#", "#.#"],
        "L": ["#..", "#..", "#..", "#..", "###"],
        "M": ["#.#", "###", "#.#", "#.#", "#.#"],
        "N": ["#.#", "###", "###", "#.#", "#.#"],
        "O": [".#.", "#.#", "#.#", "#.#", ".#."],
        "P": ["##.", "#.#", "##.", "#..", "#.."],
        "Q": [".#.", "#.#", "#.#", "##.", ".##"],
        "R": ["##.", "#.#", "##.", "#.#", "#.#"],
        "S": [".##", "#..", ".#.", "..#", "##."],
        "T": ["###", ".#.", ".#.", ".#.", ".#."],
        "U": ["#.#", "#.#", "#.#", "#.#", "###"],
        "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
        "W": ["#.#", "#.#", "#.#", "###", "#.#"],
        "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
        "Y": ["#.#", "#.#", ".#.", ".#.", ".#."],
        "Z": ["###", "..#", ".#.", "#..", "###"],
        "0": ["###", "#.#", "#.#", "#.#", "###"],
        "1": [".#.", "##.", ".#.", ".#.", "###"],
        "2": ["##.", "..#", ".#.", "#..", "###"],
        "3": ["##.", "..#", ".#.", "..#", "##."],
        "4": ["#.#", "#.#", "###", "..#", "..#"],
        "5": ["###", "#..", "##.", "..#", "##."],
        "6": [".##", "#..", "###", "#.#", "###"],
        "7": ["###", "..#", ".#.", ".#.", ".#."],
        "8": ["###", "#.#", "###", "#.#", "###"],
        "9": ["###", "#.#", "###", "..#", "##."],
        "-": ["...", "...", "###", "...", "..."],
        ".": ["...", "...", "...", "...", ".#."],
        "!": [".#.", ".#.", ".#.", "...", ".#."]
    })

    readonly property string name: {
        const wanted = String(Cfg.get("office.doorName", "") || App.userName).toUpperCase()
        const usable = Array.from(wanted).filter(ch => ch === " " || plate.font[ch] !== undefined).join("").trim()
        // "Ada Lovelace" does not fit; "ADA" does, and reads better than "ADA LOV".
        const fitted = usable.length > 7 ? usable.split(/\s+/)[0] : usable
        return fitted.slice(0, 7)
    }

    /** One rectangle per lit pixel; a name is at most 7 x 15 of them. */
    readonly property var pixels: {
        const out = []
        const left = Math.floor((plate.plateWidth - (plate.name.length * 4 - 1)) / 2)
        for (let i = 0; i < plate.name.length; ++i) {
            const glyph = plate.font[plate.name[i]]
            if (!glyph) {
                continue // a space
            }
            for (let row = 0; row < 5; ++row) {
                for (let col = 0; col < 3; ++col) {
                    if (glyph[row][col] === "#") {
                        out.push({ x: left + i * 4 + col, y: row })
                    }
                }
            }
        }
        return out
    }

    width: plateWidth
    height: 5

    Repeater {
        model: plate.pixels

        Rectangle {
            required property var modelData

            x: modelData.x
            y: modelData.y
            width: 1
            height: 1
            color: plate.ink
        }
    }
}
