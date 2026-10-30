/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "claudehook.h"
#include "officeevent.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

namespace
{
constexpr int ConnectTimeoutMs = 300;
constexpr int FlushTimeoutMs = 300;

void sendLine(const QString &path, const QByteArray &line)
{
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(ConnectTimeoutMs)) {
        return;
    }
    socket.write(line);
    socket.waitForBytesWritten(FlushTimeoutMs);
    socket.disconnectFromServer();
}
}

int runClaudeHook()
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray payload = input.readAll();

    const QJsonObject hookInput = QJsonDocument::fromJson(payload).object();
    const QJsonObject event = OfficeEvent::compact(hookInput);
    const QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';

    for (const QString &path : OfficeEvent::candidateSocketPaths()) {
        sendLine(path, line);
    }
    return 0;
}
