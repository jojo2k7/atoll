#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws every office sprite deterministically into data/office/ and writes
manifest.json + layout.json. See docs/office-art.md for the file formats.

Usage: gen-office-sprites.py [--preview OUT.png [--characters OUT.png]]
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from office_art import layout  # noqa: E402


def main(argv):
    layout.write()
    if "--preview" in argv:
        from office_art import preview
        out = argv[argv.index("--preview") + 1]
        preview.scene(out)
        if "--characters" in argv:
            preview.character_sheet(argv[argv.index("--characters") + 1])


if __name__ == "__main__":
    main(sys.argv[1:])
