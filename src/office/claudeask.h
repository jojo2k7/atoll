/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QJsonObject>

class QLocalSocket;

/**
 * `atoll --claude-ask`: the synchronous Claude Code hook for AskUserQuestion.
 *
 * Reads one PreToolUse payload from standard input, then tries each office
 * socket it finds in turn: it sends an `Ask` event and waits up to 1.5 s for
 * `{"id":…,"ack":true}` to come back on the same connection, proof that the
 * other end actually understands "Ask" (an island that doesn't - e.g. a v1
 * build - would otherwise leave this blocked for up to 590 s for nothing).
 * No ack means try the next socket; once a socket acks, that connection is
 * kept open, waiting for the real reply line (max 590 s more). On every
 * failure path - nothing reachable, nothing acks, a cancelled ask, a dropped
 * connection, or a timeout - it prints nothing and exits 0, which makes
 * Claude Code fall back to asking in the terminal as usual. See
 * docs/office-v2.md section 3.
 */
int runClaudeAsk();

namespace ClaudeAsk
{
/**
 * The ack-then-reply handshake on one already-connected socket. Writes
 * `ask` (which must already carry its `id`), waits up to `ackTimeoutMs` for
 * a matching `{"id":…,"ack":true}`, and - only once acked - waits up to
 * `replyTimeoutMs` more for the real reply line.
 *
 * `*acked` (when non-null) reports whether the ack stage succeeded, which is
 * what the caller needs to decide between "try the next socket" (no ack)
 * and "give up entirely" (acked, but no reply arrived). The return value is
 * the parsed reply object, or an empty object on any failure at either
 * stage (no ack, a malformed line, a dropped connection, or a timeout).
 *
 * Exposed for tests with short timeouts; `runClaudeAsk()` always calls this
 * with the real ones (1500 ms, 590000 ms).
 */
QJsonObject negotiate(QLocalSocket &socket, const QJsonObject &ask, int ackTimeoutMs, int replyTimeoutMs, bool *acked = nullptr);
}
