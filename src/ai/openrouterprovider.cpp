/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "openrouterprovider.h"

#include <QJsonDocument>
#include <QNetworkRequest>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView DefaultBase{"https://openrouter.ai/api/v1"};
// OpenRouter asks to be told who is calling; it is etiquette, not auth.
constexpr QLatin1StringView Referer{"https://github.com/atoll-shell/atoll"};
} // namespace

OpenRouterProvider::OpenRouterProvider(QNetworkAccessManager *network, QObject *parent)
    : AiProvider(network, parent)
{
}

QNetworkRequest OpenRouterProvider::buildRequest(const AiRequest &request) const
{
    const QString base = request.baseUrl.isEmpty()
        ? QString(DefaultBase)
        : QString(request.baseUrl).remove(QRegularExpression(u"/+$"_s));
    QNetworkRequest network{QUrl(base + u"/chat/completions"_s)};
    network.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    network.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    network.setRawHeader("HTTP-Referer", QString(Referer).toUtf8());
    network.setRawHeader("X-Title", "Atoll");
    network.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return network;
}

QJsonValue OpenRouterProvider::imagePart(const QByteArray &data, const QString &mediaType)
{
    return QJsonObject{{u"type"_s, u"image_url"_s},
                       {u"image_url"_s,
                        QJsonObject{{u"url"_s,
                                     u"data:%1;base64,%2"_s.arg(mediaType,
                                                                QString::fromLatin1(data.toBase64()))}}}};
}

QByteArray OpenRouterProvider::buildBody(const AiRequest &request) const
{
    QJsonArray messages;

    if (!request.systemPrompt.isEmpty()) {
        messages.append(QJsonObject{{u"role"_s, u"system"_s}, {u"content"_s, request.systemPrompt}});
    }

    for (const AiTurn &turn : request.history) {
        if (turn.role == u"assistant"_s) {
            // A turn this provider wrote goes back exactly as it arrived;
            // anything else is rebuilt in this dialect.
            if (!turn.rawContent.isEmpty() && turn.rawProvider == id()) {
                messages.append(turn.rawContent.first());
                continue;
            }
            QJsonObject message{{u"role"_s, u"assistant"_s}};
            message.insert(u"content"_s, turn.text.isEmpty() ? QJsonValue() : QJsonValue(turn.text));
            if (!turn.toolCalls.isEmpty()) {
                QJsonArray calls;
                for (const AiToolCall &call : turn.toolCalls) {
                    // Arguments travel as a JSON string, not an object.
                    const QString arguments = QString::fromUtf8(
                        QJsonDocument(QJsonObject::fromVariantMap(call.input)).toJson(QJsonDocument::Compact));
                    calls.append(QJsonObject{
                        {u"id"_s, call.id},
                        {u"type"_s, u"function"_s},
                        {u"function"_s, QJsonObject{{u"name"_s, call.name}, {u"arguments"_s, arguments}}}});
                }
                message.insert(u"tool_calls"_s, calls);
            }
            messages.append(message);
            continue;
        }

        // Tool answers are messages of their own, and have to follow the
        // assistant turn that asked for them directly.
        for (const AiToolResult &result : turn.toolResults) {
            // An image cannot ride in a tool message, so it follows as a part
            // of the next user message instead.
            QJsonObject answer{{u"role"_s, u"tool"_s},
                               {u"tool_call_id"_s, result.id},
                               {u"content"_s,
                                result.image.isEmpty()
                                    ? QJsonValue(result.content)
                                    : QJsonValue(result.content + u"\n\n(A picture was produced; it follows.)"_s)}};
            if (result.isError) {
                answer.insert(u"is_error"_s, true);
            }
            messages.append(answer);
            if (!result.image.isEmpty()) {
                messages.append(QJsonObject{
                    {u"role"_s, u"user"_s},
                    {u"content"_s, QJsonArray{imagePart(result.image, result.imageMediaType)}}});
            }
        }

        QJsonArray parts;
        if (!turn.image.isEmpty()) {
            parts.append(imagePart(turn.image, turn.imageMediaType));
        }
        if (!turn.text.isEmpty()) {
            if (parts.isEmpty()) {
                messages.append(QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, turn.text}});
            } else {
                parts.append(QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, turn.text}});
                messages.append(QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, parts}});
            }
        } else if (!parts.isEmpty()) {
            messages.append(QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, parts}});
        }
    }

    QJsonArray tools;
    for (const QJsonValue &entry : request.tools) {
        const QJsonObject definition = entry.toObject();
        tools.append(QJsonObject{
            {u"type"_s, u"function"_s},
            {u"function"_s,
             QJsonObject{{u"name"_s, definition.value(u"name"_s)},
                         {u"description"_s, definition.value(u"description"_s)},
                         {u"parameters"_s, definition.value(u"input_schema"_s)}}}});
    }

    QString model = request.model.isEmpty() ? defaultModel() : request.model;
    if (request.webSearch) {
        // Web search rides on the model name as OpenRouter's ":online"
        // suffix - which replaces whatever choice suffix is already there
        // (":free", ":extended"), because two of them in a row make an id no
        // endpoint answers to.
        const qsizetype colon = model.indexOf(u':');
        if (colon >= 0) {
            model.truncate(colon);
        }
        model += u":online"_s;
    }

    QJsonObject body{{u"model"_s, model},
                     {u"messages"_s, messages},
                     {u"max_tokens"_s, request.maxTokens},
                     {u"stream"_s, true}};
    if (!tools.isEmpty()) {
        body.insert(u"tools"_s, tools);
    }

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void OpenRouterProvider::resetTurn()
{
    m_partialCalls.clear();
    m_calls.clear();
    m_raw = {};
    m_text.clear();
}

void OpenRouterProvider::handleEvent(const QJsonObject &event)
{
    if (event.contains(u"error"_s)) {
        Q_EMIT failed(event.value(u"error"_s)
                          .toObject()
                          .value(u"message"_s)
                          .toString(tr("The assistant service reported an error.")));
        return;
    }

    const QJsonArray choices = event.value(u"choices"_s).toArray();
    if (choices.isEmpty()) {
        return;
    }
    const QJsonObject choice = choices.first().toObject();
    const QJsonObject delta = choice.value(u"delta"_s).toObject();

    // Reasoning tokens travel under "reasoning"; some models behind OpenRouter
    // use the DeepSeek spelling instead.
    const QString reasoning = delta.value(u"reasoning"_s).toString();
    if (!reasoning.isEmpty()) {
        Q_EMIT thoughtDelta(reasoning);
    } else {
        const QString altReasoning = delta.value(u"reasoning_content"_s).toString();
        if (!altReasoning.isEmpty()) {
            Q_EMIT thoughtDelta(altReasoning);
        }
    }

    const QString text = delta.value(u"content"_s).toString();
    if (!text.isEmpty()) {
        m_text += text;
        Q_EMIT textDelta(text);
    }

    // Tool calls arrive argument-fragment by argument-fragment, matched up by
    // index; the id and name ride in on the first fragment. A backend that
    // omits the index has exactly one call in flight, which is index zero.
    const auto fragments = delta.value(u"tool_calls"_s).toArray();
    for (const QJsonValue &entry : fragments) {
        const QJsonObject fragment = entry.toObject();
        const int index = fragment.contains(u"index"_s) ? fragment.value(u"index").toInt() : 0;
        PartialCall &partial = m_partialCalls[index];
        const QString id = fragment.value(u"id"_s).toString();
        if (!id.isEmpty()) {
            partial.id = id;
        }
        const QString name = fragment.value(u"function"_s).toObject().value(u"name"_s).toString();
        if (!name.isEmpty()) {
            partial.name = name;
        }
        partial.arguments += fragment.value(u"function"_s).toObject().value(u"arguments"_s).toString();
    }

    const QString finish = choice.value(u"finish_reason"_s).toString();
    if (finish.isEmpty()) {
        return;
    }

    QJsonArray rawCalls;
    for (auto it = m_partialCalls.cbegin(); it != m_partialCalls.cend(); ++it) {
        AiToolCall call;
        call.id = it->id.isEmpty() ? u"or%1"_s.arg(it.key()) : it->id;
        call.name = it->name;
        const QJsonDocument parsed = QJsonDocument::fromJson(it->arguments.toUtf8());
        call.input = parsed.isObject() ? parsed.object().toVariantMap() : QVariantMap{};
        m_calls.append(call);

        rawCalls.append(QJsonObject{
            {u"id"_s, call.id},
            {u"type"_s, u"function"_s},
            {u"function"_s, QJsonObject{{u"name"_s, call.name}, {u"arguments"_s, it->arguments}}}});
    }

    // One message, whole: replaying the conversation means handing this exact
    // object back the next time round.
    QJsonObject replayed{{u"role"_s, u"assistant"_s}};
    replayed.insert(u"content"_s, m_text.isEmpty() ? QJsonValue() : QJsonValue(m_text));
    if (!rawCalls.isEmpty()) {
        replayed.insert(u"tool_calls"_s, rawCalls);
    }
    m_raw.append(replayed);

    QString stopReason = u"end_turn"_s;
    if (!m_calls.isEmpty()) {
        stopReason = u"tool_use"_s;
    } else if (finish == u"length"_s) {
        stopReason = u"max_tokens"_s;
    } else if (finish == u"content_filter"_s) {
        stopReason = u"refusal"_s;
    }

    Q_EMIT turnEnded(stopReason, m_calls, m_raw);
    resetTurn();
}
