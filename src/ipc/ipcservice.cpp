#include "app/application.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ipcservice.h"

#include "ai/screencapture.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>

using namespace Qt::StringLiterals;

IpcService::IpcService(QObject *parent)
    : QObject(parent)
{
}

bool IpcService::registerOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(u"org.atoll.Atoll"_s)) {
        return false;
    }
    return bus.registerObject(u"/Atoll"_s,
                              this,
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
}

bool IpcService::registerSettingsOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(u"org.atoll.AtollSettings"_s)) {
        return false;
    }
    return bus.registerObject(u"/Settings"_s,
                              this,
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
}

void IpcService::setWindowActive(bool active)
{
    if (m_windowActive == active) {
        return;
    }
    m_windowActive = active;
    Q_EMIT windowActiveChanged(m_windowActive);
}

void IpcService::expand()
{
    Q_EMIT expandRequested();
}

void IpcService::collapse()
{
    Q_EMIT collapseRequested();
}

void IpcService::toggle()
{
    Q_EMIT toggleRequested();
}

void IpcService::showText(const QString &icon, const QString &text)
{
    Q_EMIT textRequested(icon, text);
}

void IpcService::showProgress(const QString &icon, int percent, const QString &text)
{
    Q_EMIT progressRequested(icon, percent, text);
}

void IpcService::share(const QStringList &paths)
{
    Q_EMIT shareRequested(paths);
}

void IpcService::assistant()
{
    Q_EMIT assistantRequested();
}

void IpcService::ask(const QString &prompt)
{
    Q_EMIT askRequested(prompt);
}

QString IpcService::park(const QString &kind)
{
    // The answer depends on a person - approving a step, or agreeing to let
    // the screen be looked at - so the method call is set aside and the reply
    // sent whenever that person gets to it.
    setDelayedReply(true);
    const QString token = u"%1-%2"_s.arg(kind).arg(++m_reviewCounter);
    m_reviews.insert(token, OpenReview{connection(), message()});
    return token;
}

void IpcService::sendReply(const QString &token, const QString &value)
{
    const auto waiting = m_reviews.take(token);
    if (waiting.request.type() == QDBusMessage::InvalidMessage) {
        return;
    }
    QDBusConnection connection = waiting.connection;
    connection.send(waiting.request.createReply(value));
}

QString IpcService::reviewToolCall(const QString &payload)
{
    if (!calledFromDBus()) {
        return {};
    }
    Q_EMIT toolReviewRequested(payload, park(u"review"_s));
    return {};
}

QString IpcService::captureScreen(const QString &screen)
{
    if (!calledFromDBus()) {
        return {};
    }
    Q_EMIT screenCaptureRequested(park(u"shot"_s), screen);
    return {};
}

QString IpcService::askUser(const QString &question, const QStringList &options)
{
    if (!calledFromDBus()) {
        return {};
    }
    Q_EMIT userChoiceRequested(park(u"ask"_s), question, options);
    return {};
}

QString IpcService::listScreens()
{
    // Straight from the windowing system: this is a fact about the machine,
    // not about the assistant, and nothing here has to be asked of anybody.
    QStringList lines;
    const QVariantList outputs = ScreenCapture::outputs();
    for (const QVariant &entry : outputs) {
        const QVariantMap output = entry.toMap();
        lines.append(u"%1 %2x%3%4"_s.arg(output.value(u"name"_s).toString())
                         .arg(output.value(u"width"_s).toInt())
                         .arg(output.value(u"height"_s).toInt())
                         .arg(output.value(u"primary"_s).toBool() ? u" (main)"_s : QString()));
    }
    return lines.join(u'\n');
}

void IpcService::answerToolReview(const QString &token, const QString &verdictJson)
{
    sendReply(token, verdictJson);
}

void IpcService::answerScreenCapture(const QString &token, const QString &result)
{
    sendReply(token, result);
}

void IpcService::answerUserChoice(const QString &token, const QString &result)
{
    sendReply(token, result);
}

void IpcService::dismiss()
{
    Q_EMIT dismissRequested();
}

void IpcService::office()
{
    Q_EMIT officeRequested();
}

void IpcService::action(const QString &id)
{
    Q_EMIT actionRequested(id);
}

void IpcService::reloadConfig()
{
    Q_EMIT reloadRequested();
}

void IpcService::settings()
{
    Q_EMIT settingsRequested();
}

void IpcService::raise()
{
    Q_EMIT raiseRequested();
}

QString IpcService::version() const
{
    return QStringLiteral(ATOLL_VERSION);
}

void IpcService::quit()
{
    QCoreApplication::quit();
}

void IpcService::setDodgeHelper(bool enabled)
{
    // The helper is a KWin script that reports whether an application window
    // has the focus. It only runs while the island is set to dodge windows,
    // and it is never written anywhere: it ships with Atoll.
    if (m_dodgeHelper == int(enabled)) {
        return; // every island asks; once is enough
    }
    m_dodgeHelper = int(enabled);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.interface()->isServiceRegistered(u"org.kde.KWin"_s)) {
        return;
    }
    const auto scripting = [&bus](const QString &method, const QVariantList &args) {
        QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/Scripting"_s,
                                                              u"org.kde.kwin.Scripting"_s, method);
        message.setArguments(args);
        return bus.call(message);
    };

    const QDBusMessage loaded = scripting(u"isScriptLoaded"_s, {u"atoll-dodge"_s});
    const bool isLoaded = loaded.type() == QDBusMessage::ReplyMessage && !loaded.arguments().isEmpty()
                          && loaded.arguments().constFirst().toBool();
    if (isLoaded) {
        // A copy left over from an earlier run would not report again until
        // the focus moves, so start from a fresh one.
        scripting(u"unloadScript"_s, {u"atoll-dodge"_s});
    }
    if (!enabled) {
        setWindowActive(false);
        return;
    }

    const QString relative = u"kwin/scripts/atoll-dodge/contents/code/main.js"_s;
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative);
    if (path.isEmpty()) {
        // Running from a build directory: the script is still in the source tree.
        const QString fromBuild = QCoreApplication::applicationDirPath() + u"/../data/"_s + relative;
        if (QFile::exists(fromBuild)) {
            path = QFileInfo(fromBuild).canonicalFilePath();
        }
    }
    if (path.isEmpty()) {
        qWarning("atoll: the window dodge helper is not installed; the island cannot dodge windows");
        return;
    }
    if (scripting(u"loadScript"_s, {path, u"atoll-dodge"_s}).type() == QDBusMessage::ReplyMessage) {
        scripting(u"start"_s, {});
    }
}


QString IpcService::focus(const QString &action)
{
    auto *focus = Application::instance()->focus();
    if (action == "start") focus->start();
    else if (action == "pause") focus->togglePause();
    else if (action == "stop") focus->stop();
    else if (action == "extend") focus->extend();
    else if (action == "next") focus->next();
    else if (action != "status") return "error: use start, pause, stop, extend, next or status";
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(focus->state())).toJson(QJsonDocument::Compact));
}

QString IpcService::audioOutputs() const
{
    const auto *audio = Application::instance()->audio();
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"outputs", QJsonArray::fromVariantList(audio->outputs())},
        {"current", audio->currentId()}, {"busy", audio->busy()}, {"message", audio->message()}
    }).toJson(QJsonDocument::Compact));
}
void IpcService::selectAudioOutput(const QString &id) { Application::instance()->audio()->selectOutput(id); }
void IpcService::releaseAudio() { Application::instance()->audio()->releaseBluetooth(); }
