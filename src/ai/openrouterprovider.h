/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"

#include <QMap>

/**
 * OpenRouter, whose catalogue of models is reached through one
 * OpenAI-compatible chat completions endpoint.
 *
 * One key, many providers' models - which also means model names carry a
 * vendor prefix ("anthropic/claude-sonnet-4"), and that the web search switch
 * rides along as OpenRouter's ":online" suffix rather than as a tool.
 */
class OpenRouterProvider : public AiProvider
{
    Q_OBJECT

public:
    explicit OpenRouterProvider(QNetworkAccessManager *network, QObject *parent = nullptr);

    QString id() const override
    {
        return QStringLiteral("openrouter");
    }
    QString defaultModel() const override
    {
        return QStringLiteral("anthropic/claude-sonnet-4");
    }

protected:
    QNetworkRequest buildRequest(const AiRequest &request) const override;
    QByteArray buildBody(const AiRequest &request) const override;
    void handleEvent(const QJsonObject &event) override;
    void resetTurn() override;

private:
    /** A tool call being streamed in pieces, keyed by its index. */
    struct PartialCall {
        QString id;
        QString name;
        QString arguments;
    };

    static QJsonValue imagePart(const QByteArray &data, const QString &mediaType);

    QMap<int, PartialCall> m_partialCalls;
    QList<AiToolCall> m_calls;
    QJsonArray m_raw;
    QString m_text;
};
