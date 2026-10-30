/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/**
 * Turns a Claude Code hook payload into the compact single-line event that
 * travels over the office socket, and knows where that socket lives.
 *
 * Shared between `atoll --claude-hook` (which only ever writes) and
 * `OfficeService` (which only ever reads what `compact()` produced), so the
 * two sides cannot drift apart on the event shape.
 */
namespace OfficeEvent
{
/**
 * See docs/office.md "Event (`"v": 1`)" for the field table and `arg` rules,
 * and docs/office-v2.md section 2 for the `pv` (monitor preview) table.
 * `pv` construction is defensive by design: response shapes for several
 * tools are undocumented, so odd or missing fields degrade to an empty or
 * partial preview rather than ever crashing the hook.
 */
QJsonObject compact(const QJsonObject &hookInput);

/** `$XDG_RUNTIME_DIR/atoll/claude.sock`, empty if there is no runtime dir. */
QString primarySocketPath();

/**
 * Every office socket that currently exists: the primary one first, then
 * whatever `QDir` finds under `/run/user/*\/atoll/claude.sock`, deduplicated.
 * A hook running as root has no primary of its own but still reaches the
 * user's island through this list.
 */
QStringList candidateSocketPaths();
}
