// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Text in the office's pixel font at one font pixel per logical pixel
 * (monogram at 16 px: cap height 7, advance 6, line height 10).
 * `y` is the top of the capitals, like the art doc's label points.
 */
Text {
    font.family: "monogram"
    font.pixelSize: 16
    font.hintingPreference: Font.PreferFullHinting
    renderType: Text.NativeRendering
    color: OfficeArt.text
    lineHeightMode: Text.FixedHeight
    lineHeight: 10
    // The font's ascent puts the capitals 4 px below the top; pull them up so
    // `y` is the cap top while each line still takes 10 px.
    topPadding: -4
    bottomPadding: 4
    textFormat: Text.PlainText
}
