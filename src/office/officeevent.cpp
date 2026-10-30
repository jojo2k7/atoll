/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "officeevent.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

namespace
{
/** Newlines collapse to spaces; anything longer than `maxLength` is cut with "…". */
QString shorten(QString text, int maxLength)
{
    text.replace(u'\n', u' ').replace(u'\r', u' ');
    text = text.trimmed();
    if (text.length() > maxLength) {
        text = text.left(maxLength - 1) + u"…"_s;
    }
    return text;
}

/** First non-empty string field, trying each key in order. */
QString firstOf(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = object.value(key).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

/**
 * `arg`, built from `tool_name`/`tool_input` per docs/office.md. `to` and
 * `msg` come out of the same tools, so they are filled in here too rather
 * than re-parsed by the caller.
 */
QString toolArgument(const QString &toolName, const QJsonObject &toolInput, QString &to, QString &msg)
{
    if (toolName == u"Bash"_s) {
        return firstOf(toolInput, {u"description"_s, u"command"_s});
    }
    if (toolName == u"Read"_s || toolName == u"Edit"_s || toolName == u"Write"_s || toolName == u"MultiEdit"_s) {
        return QFileInfo(toolInput.value(u"file_path"_s).toString()).fileName();
    }
    if (toolName == u"NotebookEdit"_s) {
        const QString path = toolInput.contains(u"notebook_path"_s) ? toolInput.value(u"notebook_path"_s).toString()
                                                                     : toolInput.value(u"file_path"_s).toString();
        return QFileInfo(path).fileName();
    }
    if (toolName == u"Grep"_s || toolName == u"Glob"_s) {
        return toolInput.value(u"pattern"_s).toString();
    }
    if (toolName == u"WebFetch"_s) {
        return QUrl(toolInput.value(u"url"_s).toString()).host();
    }
    if (toolName == u"WebSearch"_s) {
        return toolInput.value(u"query"_s).toString();
    }
    if (toolName == u"Agent"_s || toolName == u"Task"_s) {
        return toolInput.value(u"description"_s).toString();
    }
    if (toolName == u"SendMessage"_s) {
        to = firstOf(toolInput, {u"to"_s, u"recipient"_s, u"name"_s});
        msg = firstOf(toolInput, {u"message"_s, u"content"_s, u"text"_s, u"summary"_s});
        return to;
    }
    if (toolName == u"AskUserQuestion"_s) {
        const QJsonArray questions = toolInput.value(u"questions"_s).toArray();
        const QString question = questions.isEmpty() ? QString() : questions.first().toObject().value(u"question"_s).toString();
        msg = question;
        return question;
    }
    if (toolName == u"Skill"_s) {
        return toolInput.value(u"skill"_s).toString();
    }
    if (toolName == u"TaskCreate"_s || toolName == u"TaskUpdate"_s) {
        return firstOf(toolInput, {u"subject"_s, u"taskId"_s});
    }
    return {};
}

constexpr int PvMaxChars = 4000;

/** Lines split on both "\n" and a stray "\r\n", never choking on either. */
QStringList pvLines(const QString &text)
{
    QString normalized = text;
    normalized.replace(u"\r\n"_s, u"\n"_s).replace(u'\r', u'\n');
    return normalized.split(u'\n');
}

QString firstLines(const QString &text, int count)
{
    QStringList lines = pvLines(text);
    if (lines.size() > count) {
        lines = lines.mid(0, count);
    }
    return lines.join(u'\n');
}

QString lastLines(const QString &text, int count)
{
    QStringList lines = pvLines(text);
    if (lines.size() > count) {
        lines = lines.mid(lines.size() - count, count);
    }
    return lines.join(u'\n');
}

/** `tail` keeps the end (where terminal output matters most) instead of the start. */
QString capPvText(const QString &text, bool tail = false)
{
    if (text.length() <= PvMaxChars) {
        return text;
    }
    return tail ? u"…"_s + text.right(PvMaxChars - 1) : text.left(PvMaxChars - 1) + u"…"_s;
}

/**
 * Defensive, generic text extraction for response shapes that are not
 * documented (Read/Grep/Glob/WebFetch/WebSearch/anything else's
 * `tool_response`): walks strings and the common content-carrying keys of an
 * object, falling back to the JSON itself rather than ever failing.
 */
QString pvResponseText(const QJsonValue &value, int depth = 0)
{
    if (depth > 4) {
        return {};
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.value(u"file"_s).isObject()) {
            const QString fileContent = object.value(u"file"_s).toObject().value(u"content"_s).toString();
            if (!fileContent.isEmpty()) {
                return fileContent;
            }
        }
        for (const QString &key : {u"content"_s, u"text"_s, u"result"_s, u"output"_s, u"stdout"_s}) {
            const QJsonValue found = object.value(key);
            if (found.isString() && !found.toString().isEmpty()) {
                return found.toString();
            }
        }
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return {};
}

/** See docs/office-v2.md section 2 for this table. */
QJsonObject buildPv(const QString &eventName,
                     const QString &toolName,
                     const QJsonObject &toolInput,
                     const QJsonValue &toolResponse,
                     const QString &errorText)
{
    if (toolName.isEmpty()) {
        return {};
    }
    const bool isPost = eventName == u"PostToolUse"_s || eventName == u"PostToolUseFailure"_s;
    const QJsonObject responseObject = toolResponse.toObject();
    const bool failed = eventName == u"PostToolUseFailure"_s
        || (toolResponse.isObject()
            && (responseObject.value(u"is_error"_s).toBool() || !responseObject.value(u"error"_s).toString().isEmpty()));

    const auto make = [](const QString &kind, const QString &title, const QString &text, bool tail = false) {
        QJsonObject pv;
        pv[u"kind"_s] = kind;
        pv[u"title"_s] = title;
        pv[u"text"_s] = capPvText(text, tail);
        return pv;
    };

    if (toolName == u"Edit"_s || toolName == u"MultiEdit"_s) {
        const QString title = QFileInfo(toolInput.value(u"file_path"_s).toString()).fileName();
        QStringList lines;
        const auto addDiff = [&lines](const QJsonObject &edit) {
            for (const QString &line : pvLines(edit.value(u"old_string"_s).toString())) {
                lines << u"-"_s + line;
            }
            for (const QString &line : pvLines(edit.value(u"new_string"_s).toString())) {
                lines << u"+"_s + line;
            }
        };
        const QJsonValue edits = toolInput.value(u"edits"_s);
        if (edits.isArray()) {
            for (const QJsonValue &edit : edits.toArray()) {
                addDiff(edit.toObject());
            }
        } else {
            addDiff(toolInput);
        }
        if (lines.size() > 60) {
            lines = lines.mid(0, 60);
        }
        return make(u"diff"_s, title, lines.join(u'\n'));
    }
    if (toolName == u"Write"_s) {
        const QString title = QFileInfo(toolInput.value(u"file_path"_s).toString()).fileName();
        return make(u"code"_s, title, firstLines(toolInput.value(u"content"_s).toString(), 60));
    }
    if (toolName == u"NotebookEdit"_s) {
        const QString path = toolInput.contains(u"notebook_path"_s) ? toolInput.value(u"notebook_path"_s).toString()
                                                                     : toolInput.value(u"file_path"_s).toString();
        return make(u"code"_s, QFileInfo(path).fileName(), toolInput.value(u"new_source"_s).toString());
    }
    if (toolName == u"Bash"_s) {
        const QString title = firstOf(toolInput, {u"description"_s, u"command"_s});
        const QString command = toolInput.value(u"command"_s).toString();
        QString text = command.isEmpty() ? QString() : u"$ "_s + command;
        if (isPost) {
            if (failed) {
                QString error = errorText;
                if (error.isEmpty()) {
                    error = responseObject.value(u"error"_s).toString();
                }
                if (!error.isEmpty()) {
                    text += (text.isEmpty() ? QString() : u"\n"_s) + error;
                }
            } else if (toolResponse.isObject()) {
                const QString stdoutText = responseObject.value(u"stdout"_s).toString();
                const QString stderrText = responseObject.value(u"stderr"_s).toString();
                if (!stdoutText.isEmpty()) {
                    text += u"\n"_s + lastLines(stdoutText, 50);
                }
                if (!stderrText.isEmpty()) {
                    text += u"\n"_s + stderrText;
                }
            }
        }
        return make(u"terminal"_s, title, text, /*tail=*/true);
    }
    if (toolName == u"Read"_s) {
        const QString title = QFileInfo(toolInput.value(u"file_path"_s).toString()).fileName();
        if (!isPost) {
            return make(u"doc"_s, title, toolInput.value(u"file_path"_s).toString());
        }
        return make(u"doc"_s, title, firstLines(pvResponseText(toolResponse), 60));
    }
    if (toolName == u"Grep"_s || toolName == u"Glob"_s) {
        const QString title = toolInput.value(u"pattern"_s).toString();
        if (!isPost) {
            return make(u"search"_s, title, QString());
        }
        return make(u"search"_s, title, firstLines(pvResponseText(toolResponse), 40));
    }
    if (toolName == u"WebFetch"_s || toolName == u"WebSearch"_s) {
        const QString title = toolName == u"WebFetch"_s ? QUrl(toolInput.value(u"url"_s).toString()).host()
                                                          : toolInput.value(u"query"_s).toString();
        if (!isPost) {
            return make(u"web"_s, title, toolInput.value(u"prompt"_s).toString());
        }
        return make(u"web"_s, title, firstLines(pvResponseText(toolResponse), 40));
    }
    if (toolName == u"Agent"_s || toolName == u"Task"_s) {
        return make(u"brief"_s, toolInput.value(u"description"_s).toString(), firstLines(toolInput.value(u"prompt"_s).toString(), 40));
    }
    if (toolName == u"SendMessage"_s) {
        const QString to = firstOf(toolInput, {u"to"_s, u"recipient"_s, u"name"_s});
        const QString message = firstOf(toolInput, {u"message"_s, u"content"_s, u"text"_s, u"summary"_s});
        return make(u"chat"_s, to, message);
    }
    if (toolName == u"TaskCreate"_s || toolName == u"TaskUpdate"_s || toolName == u"TodoWrite"_s) {
        const QString title = firstOf(toolInput, {u"subject"_s, u"taskId"_s});
        QStringList items;
        const QJsonValue todos = toolInput.value(u"todos"_s);
        const QJsonValue tasks = toolInput.value(u"tasks"_s);
        if (todos.isArray()) {
            for (const QJsonValue &entry : todos.toArray()) {
                const QString line = firstOf(entry.toObject(), {u"content"_s, u"activeForm"_s, u"subject"_s, u"text"_s});
                if (!line.isEmpty()) {
                    items << line;
                }
            }
        } else if (tasks.isArray()) {
            for (const QJsonValue &entry : tasks.toArray()) {
                const QString line = firstOf(entry.toObject(), {u"subject"_s, u"content"_s, u"text"_s});
                if (!line.isEmpty()) {
                    items << line;
                }
            }
        } else if (!title.isEmpty()) {
            items << title;
        }
        return make(u"plan"_s, title, items.join(u'\n'));
    }
    if (toolName == u"AskUserQuestion"_s) {
        const QJsonArray questions = toolInput.value(u"questions"_s).toArray();
        QString header;
        QString text;
        if (!questions.isEmpty()) {
            const QJsonObject question = questions.first().toObject();
            header = question.value(u"header"_s).toString();
            text = question.value(u"question"_s).toString();
        }
        return make(u"question"_s, header, text);
    }
    return make(u"tool"_s, toolName, QString::fromUtf8(QJsonDocument(toolInput).toJson(QJsonDocument::Compact)));
}
}

QJsonObject OfficeEvent::compact(const QJsonObject &hookInput)
{
    QJsonObject out;
    out[u"v"_s] = 1;

    const QString eventName = hookInput.value(u"hook_event_name"_s).toString();
    out[u"e"_s] = eventName;
    out[u"s"_s] = hookInput.value(u"session_id"_s).toString();
    out[u"cwd"_s] = hookInput.value(u"cwd"_s).toString();
    out[u"a"_s] = hookInput.value(u"agent_id"_s).toString();
    out[u"at"_s] = hookInput.value(u"agent_type"_s).toString();

    const QString toolName = hookInput.value(u"tool_name"_s).toString();
    out[u"t"_s] = toolName;

    const QJsonObject toolInput = hookInput.value(u"tool_input"_s).toObject();
    QString to;
    QString msg;
    const QString arg = toolArgument(toolName, toolInput, to, msg);
    out[u"arg"_s] = shorten(arg, 120);

    const QJsonValue toolResponse = hookInput.value(u"tool_response"_s);
    if (eventName == u"PostToolUse"_s || eventName == u"PostToolUseFailure"_s) {
        bool ok = eventName != u"PostToolUseFailure"_s;
        if (toolResponse.isObject()) {
            const QJsonObject responseObject = toolResponse.toObject();
            if (responseObject.value(u"is_error"_s).toBool()
                || !responseObject.value(u"error"_s).toString().isEmpty()) {
                ok = false;
            }
        }
        out[u"ok"_s] = ok;
    }

    const QJsonObject pv = buildPv(eventName, toolName, toolInput, toolResponse, hookInput.value(u"error"_s).toString());
    if (!pv.isEmpty()) {
        out[u"pv"_s] = pv;
    }

    if (eventName == u"Notification"_s) {
        msg = hookInput.value(u"message"_s).toString();
        out[u"nt"_s] = hookInput.value(u"notification_type"_s).toString();
    }
    if (!to.isEmpty()) {
        out[u"to"_s] = to;
    }
    // UserPromptSubmit never reaches here with a non-empty msg: it is not a
    // tool call and carries no Notification message, so there is nothing to
    // forward - which is the point, the prompt text itself stays private.
    if (eventName != u"UserPromptSubmit"_s && !msg.isEmpty()) {
        out[u"msg"_s] = shorten(msg, 200);
    }

    out[u"user"_s] = QString::fromLocal8Bit(qgetenv("USER"));
#ifdef Q_OS_UNIX
    out[u"ppid"_s] = qint64(getppid());
#else
    out[u"ppid"_s] = 0;
#endif
    out[u"ts"_s] = QDateTime::currentMSecsSinceEpoch();

    return out;
}

QString OfficeEvent::primarySocketPath()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        return {};
    }
    return runtimeDir + u"/atoll/claude.sock"_s;
}

QStringList OfficeEvent::candidateSocketPaths()
{
    QStringList found;

    const QString primary = primarySocketPath();
    if (!primary.isEmpty() && QFile::exists(primary)) {
        found.append(primary);
    }

    QDir runtimeRoot(u"/run/user"_s);
    const QFileInfoList users = runtimeRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &user : users) {
        const QString candidate = user.absoluteFilePath() + u"/atoll/claude.sock"_s;
        if (!found.contains(candidate) && QFile::exists(candidate)) {
            found.append(candidate);
        }
    }
    return found;
}
