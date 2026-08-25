/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aiprovider.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

AiProvider::AiProvider(QNetworkAccessManager *network, QObject *parent)
    : AiBackend(parent)
    , m_network(network)
{
}

AiProvider::~AiProvider()
{
    abort();
}

bool AiProvider::busy() const
{
    return !m_reply.isNull();
}

void AiProvider::abort()
{
    if (m_reply) {
        m_aborting = true;
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
        m_aborting = false;
    }
    m_buffer.clear();
}

QString AiProvider::describeHttpError(int status, const QByteArray &body)
{
    const auto document = QJsonDocument::fromJson(body);
    const QJsonObject object = document.object();

    // Anthropic nests it under "error", Google under "error" too but with a
    // different shape; both keep the sentence in "message".
    const QJsonValue error = object.value(u"error"_s);
    QString message;
    if (error.isObject()) {
        message = error.toObject().value(u"message"_s).toString();
    } else if (error.isString()) {
        message = error.toString();
    } else if (object.contains(u"message"_s)) {
        message = object.value(u"message"_s).toString();
    } else if (document.isArray() && !document.array().isEmpty()) {
        message = document.array().first().toObject().value(u"error"_s).toObject().value(u"message"_s).toString();
    }

    if (message.isEmpty()) {
        message = QString::fromUtf8(body).trimmed().left(300);
    }

    // The service's own sentence rides along wherever there is one: "the key
    // was rejected" covers a typo, an expired subscription and an unpaid bill
    // alike, and only the provider knows which.
    switch (status) {
    case 401:
    case 403:
        return message.isEmpty()
            ? QObject::tr("The API key was rejected. Check it in Atoll's settings.")
            : QObject::tr("The API key was rejected: %1").arg(message);
    case 429:
        return message.isEmpty()
            ? QObject::tr("The service is rate limiting this key. Try again in a moment.")
            : QObject::tr("Rate limited: %1").arg(message);
    case 400:
        return message.isEmpty() ? QObject::tr("The request was refused.") : message;
    default:
        break;
    }
    if (status >= 500) {
        return QObject::tr("The service is having trouble (%1). Try again shortly.").arg(status);
    }
    return message.isEmpty() ? QObject::tr("Request failed with status %1.").arg(status) : message;
}

void AiProvider::send(const AiRequest &request)
{
    abort();
    resetTurn();

    if (m_apiKey.isEmpty()) {
        Q_EMIT failed(tr("No API key is set for this provider."));
        return;
    }

    m_reply = m_network->post(buildRequest(request), buildBody(request));

    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!m_reply) {
            return;
        }
        // An error response is ordinary JSON, not an event stream. Feeding it
        // to the SSE reader would drop every line of it on the floor, and the
        // failure would then be reported with no reason attached.
        if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400) {
            m_buffer.append(m_reply->readAll());
            return;
        }
        consume();
    });

    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (m_aborting || !m_reply) {
            return;
        }
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError error = m_reply->error();
        const QString errorString = m_reply->errorString();

        if (status >= 400) {
            m_buffer.append(m_reply->readAll());
        } else {
            // Drain what arrived with the last chunk: `finished` can carry the
            // closing events, and dropping them means a turn that never ends.
            consume();
        }

        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();

        if (error == QNetworkReply::OperationCanceledError) {
            m_buffer.clear();
            return;
        }
        if (status >= 400) {
            const QByteArray body = m_buffer;
            m_buffer.clear();
            Q_EMIT failed(describeHttpError(status, body));
            return;
        }
        if (error != QNetworkReply::NoError) {
            m_buffer.clear();
            Q_EMIT failed(tr("Cannot reach the assistant service: %1").arg(errorString));
            return;
        }
        m_buffer.clear();
    });
}

void AiProvider::consume()
{
    if (!m_reply) {
        return;
    }
    m_buffer.append(m_reply->readAll());

    // Server-sent events are separated by a blank line, but every payload we
    // care about fits on one `data:` line, so the framing reduces to reading
    // whole lines and ignoring everything else.
    int newline = m_buffer.indexOf('\n');
    while (newline >= 0) {
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        newline = m_buffer.indexOf('\n');

        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.startsWith("data:")) {
            continue;
        }
        const QByteArray payload = line.mid(5).trimmed();
        if (payload.isEmpty() || payload == "[DONE]") {
            continue;
        }
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        handleEvent(document.object());
    }
}
