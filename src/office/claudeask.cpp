/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "claudeask.h"
#include "officeevent.h"

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QRandomGenerator>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace
{
constexpr int ConnectTimeoutMs = 300;
constexpr int WriteTimeoutMs = 300;
constexpr int AckTimeoutMs = 1500;
constexpr int ReplyTimeoutMs = 590 * 1000;

QString generateId()
{
    return u"ask-%1-%2"_s.arg(QDateTime::currentMSecsSinceEpoch()).arg(QRandomGenerator::global()->bounded(1000000));
}

/** Reads one more `\n`-terminated line into `outLine`, blocking at most `timeoutMs` total. */
bool readLine(QLocalSocket &socket, QByteArray &buffer, int timeoutMs, QByteArray &outLine)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (buffer.indexOf('\n') == -1) {
        const int remaining = timeoutMs - int(elapsed.elapsed());
        if (remaining <= 0) {
            return false;
        }
        if (!socket.waitForReadyRead(remaining)) {
            return false; // timeout, or the connection dropped
        }
        buffer += socket.readAll();
    }
    const int newline = buffer.indexOf('\n');
    outLine = buffer.left(newline);
    buffer.remove(0, newline + 1);
    return true;
}
}

QJsonObject ClaudeAsk::negotiate(QLocalSocket &socket, const QJsonObject &ask, int ackTimeoutMs, int replyTimeoutMs, bool *acked)
{
    if (acked) {
        *acked = false;
    }

    const QString id = ask.value(u"id"_s).toString();
    const QByteArray line = QJsonDocument(ask).toJson(QJsonDocument::Compact) + '\n';
    socket.write(line);
    if (!socket.waitForBytesWritten(WriteTimeoutMs)) {
        return {};
    }

    QByteArray buffer;
    QByteArray ackLine;
    if (!readLine(socket, buffer, ackTimeoutMs, ackLine)) {
        return {}; // nothing on the other end understood "Ask" in time
    }
    const QJsonObject ack = QJsonDocument::fromJson(ackLine).object();
    if (ack.value(u"id"_s).toString() != id || !ack.value(u"ack"_s).toBool()) {
        return {};
    }
    if (acked) {
        *acked = true;
    }

    // Acked: committed to this connection now, even if nothing replies.
    QByteArray replyLine;
    if (!readLine(socket, buffer, replyTimeoutMs, replyLine)) {
        return {};
    }
    return QJsonDocument::fromJson(replyLine).object();
}

int runClaudeAsk()
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray payload = input.readAll();
    const QJsonObject hookInput = QJsonDocument::fromJson(payload).object();
    const QJsonObject toolInput = hookInput.value(u"tool_input"_s).toObject();
    const QJsonArray questions = toolInput.value(u"questions"_s).toArray();
    if (questions.isEmpty()) {
        return 0;
    }

    const QStringList sockets = OfficeEvent::candidateSocketPaths();

    for (const QString &path : sockets) {
        QLocalSocket socket;
        socket.connectToServer(path);
        if (!socket.waitForConnected(ConnectTimeoutMs)) {
            continue;
        }

        const QString id = generateId();
        QJsonObject ask;
        ask[u"v"_s] = 1;
        ask[u"e"_s] = u"Ask"_s;
        ask[u"id"_s] = id;
        ask[u"s"_s] = hookInput.value(u"session_id"_s).toString();
        ask[u"a"_s] = hookInput.value(u"agent_id"_s).toString();
        ask[u"at"_s] = hookInput.value(u"agent_type"_s).toString();
        ask[u"cwd"_s] = hookInput.value(u"cwd"_s).toString();
        ask[u"user"_s] = QString::fromLocal8Bit(qgetenv("USER"));
        ask[u"ts"_s] = QDateTime::currentMSecsSinceEpoch();
        ask[u"questions"_s] = questions;

        bool acked = false;
        const QJsonObject reply = ClaudeAsk::negotiate(socket, ask, AckTimeoutMs, ReplyTimeoutMs, &acked);
        if (!acked) {
            continue; // this socket never acked; the next candidate might
        }

        // Acked by exactly one island: whatever happens next, this is the
        // one that now has the ask registered, so there is no "next socket"
        // to fall back to below this point.
        if (reply.isEmpty() || reply.value(u"cancel"_s).toBool() || reply.value(u"id"_s).toString() != id) {
            return 0;
        }
        const QJsonValue answers = reply.value(u"answers"_s);
        if (!answers.isObject()) {
            return 0;
        }

        QJsonObject updatedInput;
        updatedInput[u"questions"_s] = questions;
        updatedInput[u"answers"_s] = answers;

        QJsonObject hookSpecificOutput;
        hookSpecificOutput[u"hookEventName"_s] = u"PreToolUse"_s;
        hookSpecificOutput[u"permissionDecision"_s] = u"allow"_s;
        hookSpecificOutput[u"updatedInput"_s] = updatedInput;

        QJsonObject output;
        output[u"hookSpecificOutput"_s] = hookSpecificOutput;

        QTextStream(stdout) << QString::fromUtf8(QJsonDocument(output).toJson(QJsonDocument::Compact)) << '\n';
        return 0;
    }

    return 0;
}
