/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

/**
 * `atoll --claude-hook`: the Claude Code hook that feeds the office.
 *
 * Reads one hook payload from standard input, compacts it with
 * `OfficeEvent::compact()`, and writes one line to every office socket it can
 * find. It never touches the display, never prints anything - the model pays
 * for every byte it sees, so this prints nothing - and it always exits 0: a
 * hook that fails loudly would break the very tool call it is reporting on.
 */
int runClaudeHook();
