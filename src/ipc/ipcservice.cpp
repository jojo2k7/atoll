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
#include <QDir>
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
    const bool ok = bus.registerObject(u"/Atoll"_s,
                                       this,
                                       QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
    if (ok) {
        ensureKWinScriptLoaded();
    }
    return ok;
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

void IpcService::ensureKWinScriptLoaded()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface() || !bus.interface()->isServiceRegistered(u"org.kde.KWin"_s)) {
        return;
    }

    // Check if KWin already has the script loaded
    QDBusMessage checkMsg = QDBusMessage::createMethodCall(
        u"org.kde.KWin"_s,
        u"/Scripting"_s,
        u"org.kde.kwin.Scripting"_s,
        u"isScriptLoaded"_s);
    checkMsg << u"atoll-dodge"_s;
    const QDBusMessage checkReply = bus.call(checkMsg);
    if (checkReply.type() == QDBusMessage::ReplyMessage && !checkReply.arguments().isEmpty()
        && checkReply.arguments().constFirst().toBool()) {
        return;
    }

    // Search for script file
    QString scriptPath;
    const QStringList candidates = {
        QDir::homePath() + u"/.local/share/kwin/scripts/atoll-dodge/contents/code/main.js"_s,
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"kwin/scripts/atoll-dodge/contents/code/main.js"_s),
        u"/usr/share/kwin/scripts/atoll-dodge/contents/code/main.js"_s,
        QCoreApplication::applicationDirPath() + u"/../data/kwin/scripts/atoll-dodge/contents/code/main.js"_s,
        QCoreApplication::applicationDirPath() + u"/data/kwin/scripts/atoll-dodge/contents/code/main.js"_s
    };

    for (const auto &candidate : candidates) {
        if (!candidate.isEmpty() && QFile::exists(candidate)) {
            scriptPath = candidate;
            break;
        }
    }

    // If not found, deploy to ~/.local/share/kwin/scripts/atoll-dodge automatically
    if (scriptPath.isEmpty()) {
        const QString targetDir = QDir::homePath() + u"/.local/share/kwin/scripts/atoll-dodge"_s;
        QDir().mkpath(targetDir + u"/contents/code"_s);

        QFile codeFile(targetDir + u"/contents/code/main.js"_s);
        if (codeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            static const char kCode[] =
                "function isAppActive() {\n"
                "    var win = workspace.activeWindow;\n"
                "    if (!win) return false;\n"
                "    if (win.desktopWindow || win.dock) return false;\n"
                "    var cls = (win.resourceClass || \"\").toLowerCase();\n"
                "    if (cls.indexOf(\"plasmashell\") !== -1 || cls.indexOf(\"plasma\") !== -1) return false;\n"
                "    if (cls.indexOf(\"spectacle\") !== -1 || cls.indexOf(\"atoll\") !== -1) return false;\n"
                "    if (win.minimized || win.hidden || win.hiddenByShowDesktop) return false;\n"
                "    return win.normalWindow === true || win.dialog === true;\n"
                "}\n\n"
                "function updateActive() {\n"
                "    var active = isAppActive();\n"
                "    callDBus(\"org.atoll.Atoll\", \"/Atoll\", \"org.atoll.Atoll\", \"setWindowActive\", active);\n"
                "}\n\n"
                "workspace.windowActivated.connect(updateActive);\n"
                "workspace.windowRemoved.connect(updateActive);\n"
                "if (workspace.currentDesktopChanged) {\n"
                "    workspace.currentDesktopChanged.connect(updateActive);\n"
                "}\n\n"
                "var timer = new QTimer();\n"
                "timer.interval = 300;\n"
                "timer.timeout.connect(updateActive);\n"
                "timer.start();\n\n"
                "updateActive();\n";
            codeFile.write(kCode);
            codeFile.close();
            scriptPath = codeFile.fileName();
        }

        QFile metaFile(targetDir + u"/metadata.json"_s);
        if (metaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            static const char kMeta[] =
                "{\n"
                "    \"KPackageStructure\": \"KWin/Script\",\n"
                "    \"KPlugin\": {\n"
                "        \"Id\": \"atoll-dodge\",\n"
                "        \"Name\": \"Atoll Dodge Helper\",\n"
                "        \"Description\": \"Notifies Atoll when an app window is active\",\n"
                "        \"Icon\": \"preferences-system-windows\",\n"
                "        \"Version\": \"1.0\"\n"
                "    },\n"
                "    \"X-Plasma-API\": \"javascript\"\n"
                "}\n";
            metaFile.write(kMeta);
            metaFile.close();
        }
    }

    if (!scriptPath.isEmpty()) {
        QDBusMessage loadMsg = QDBusMessage::createMethodCall(
            u"org.kde.KWin"_s,
            u"/Scripting"_s,
            u"org.kde.kwin.Scripting"_s,
            u"loadScript"_s);
        loadMsg << scriptPath << u"atoll-dodge"_s;
        const QDBusMessage loadReply = bus.call(loadMsg);
        if (loadReply.type() == QDBusMessage::ReplyMessage) {
            QDBusMessage startMsg = QDBusMessage::createMethodCall(
                u"org.kde.KWin"_s,
                u"/Scripting"_s,
                u"org.kde.kwin.Scripting"_s,
                u"start"_s);
            bus.call(startMsg);
        }
    }
}

