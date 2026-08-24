/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "application.h"

#include "ai/aiservice.h"
#include "calendar/calendarservice.h"
#include "config/config.h"
#include "dbus/dbusmonitor.h"
#include "dbus/mprismanager.h"
#include "dbus/mprisplayer.h"
#include "dbus/notificationmodel.h"
#include "dbus/notificationmonitor.h"
#include "dbus/osdmonitor.h"
#include "ipc/ipcservice.h"
#include "media/lyricsservice.h"
#include "share/shareservice.h"
#include "shellwindow.h"
#include "system/battery.h"
#include "system/clock.h"
#include "system/lockmonitor.h"
#include "system/visualizer.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
Application *s_instance = nullptr;
}

Application *Application::instance()
{
    return s_instance;
}

Application *Application::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    Q_ASSERT(s_instance);
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

Application::Application(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT_X(!s_instance, "Application", "Atoll expects exactly one Application");
    s_instance = this;

    m_config = new Config(this);
    m_config->ensureUserFile();

    m_shell = new ShellWindow(m_config, this);
    m_osd = new OsdMonitor(m_config, this);
    m_notificationMonitor = new NotificationMonitor(m_config, this);
    m_notifications = new NotificationModel(m_config, this);
    m_media = new MprisManager(m_config, this);
    m_battery = new Battery(this);
    m_clock = new Clock(m_config, this);
    m_visualizer = new Visualizer(m_config, this);
    m_lyrics = new LyricsService(m_config, m_media, this);
    m_lock = new LockMonitor(this);
    m_ipc = new IpcService(this);
    m_share = new ShareService(m_config, this);
    m_ai = new AiService(m_config, this);
    m_calendar = new CalendarService(m_config, this);

    connect(m_notificationMonitor, &NotificationMonitor::posted, m_notifications, &NotificationModel::onPosted);
    connect(m_notificationMonitor, &NotificationMonitor::idAssigned, m_notifications, &NotificationModel::onIdAssigned);
    connect(m_notificationMonitor, &NotificationMonitor::closed, m_notifications, &NotificationModel::onClosed);

    connect(m_ipc, &IpcService::textRequested, m_osd, &OsdMonitor::showText);
    connect(m_ipc, &IpcService::progressRequested, m_osd, &OsdMonitor::showProgress);
    connect(m_ipc, &IpcService::reloadRequested, m_config, &Config::reload);
    connect(m_ipc, &IpcService::settingsRequested, this, [this] {
        openSettings();
    });
    connect(m_ipc, &IpcService::shareRequested, m_share, &ShareService::offerPaths);
    connect(m_ipc, &IpcService::askRequested, m_ai, &AiService::ask);
    connect(m_ipc, &IpcService::assistantRequested, m_ai, &AiService::engage);
    // The gate the command-line client is started with reaches the island
    // here: a tool call comes in, a verdict goes back out, and in between a
    // person is asked exactly as they would be for any other backend.
    connect(m_ipc, &IpcService::toolReviewRequested, m_ai, &AiService::reviewToolCall);
    connect(m_ai, &AiService::toolReviewAnswered, m_ipc, &IpcService::answerToolReview);
    // `atollctl screenshot` - which is how the assistant looks at the screen
    // rather than asking the user to describe it - arrives the same way.
    connect(m_ipc, &IpcService::screenCaptureRequested, m_ai, &AiService::captureScreenFor);
    connect(m_ai, &AiService::screenCaptureAnswered, m_ipc, &IpcService::answerScreenCapture);
    // `atollctl choose` - the client's only way of putting a question where the
    // user can actually answer it, which is on the island as buttons.
    connect(m_ipc, &IpcService::userChoiceRequested, m_ai, &AiService::askUserFor);
    connect(m_ai, &AiService::userChoiceAnswered, m_ipc, &IpcService::answerUserChoice);

    // Whatever the assistant wants to say when nobody is looking at it goes
    // out through the island's own OSD, not through a desktop notification.
    connect(m_ai, &AiService::messageRequested, this, [this](const QString &summary, const QString &body) {
        m_osd->showText(u"dialog-information"_s, body.isEmpty() ? summary : summary + u" — "_s + body);
    });

    // The spectrum only runs while something is actually playing.
    const auto syncVisualizer = [this] {
        MprisPlayer *player = m_media->active();
        m_visualizer->setActive(player && player->playing());
    };
    connect(m_media, &MprisManager::activeChanged, this, syncVisualizer);
}

Application::~Application()
{
    s_instance = nullptr;
}

QString Application::version() const
{
    return QStringLiteral(ATOLL_VERSION);
}

QString Application::settingsPage() const
{
    return qEnvironmentVariable("ATOLL_SETTINGS_PAGE");
}

bool Application::debugSurface() const
{
    return qEnvironmentVariableIntValue("ATOLL_DEBUG_SURFACE") > 0;
}

bool Application::debugState() const
{
    return qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0;
}

void Application::start()
{
    QStringList rules = OsdMonitor::matchRules();
    rules += NotificationMonitor::matchRules(m_config->value(u"notifications.trackIds"_s, true).toBool());

    m_monitor = new DBusMonitor(this);
    connect(m_monitor, &DBusMonitor::messageReceived, this, &Application::onBusMessage);
    connect(m_monitor, &DBusMonitor::failed, this, [this](const QString &reason) {
        m_busTapActive = false;
        m_busError = reason;
        qWarning("atoll: cannot observe the session bus (%s). OSD and notification "
                 "mirroring are disabled; media control still works.",
                 qUtf8Printable(reason));
        Q_EMIT busTapChanged();
    });

    if (m_monitor->start(rules)) {
        m_busTapActive = true;
        Q_EMIT busTapChanged();
    }

    m_ipc->registerOnBus();

    // Discovery only starts once the island is really up: a settings window
    // has no business announcing itself to the network.
    m_share->start();
}

void Application::onBusMessage(const DBusMessageInfo &message)
{
    if (m_osd->handleMessage(message)) {
        return;
    }
    m_notificationMonitor->handleMessage(message);
}

void Application::adjustVolume(int deltaPercent)
{
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (!wpctl.isEmpty()) {
        const QString step = u"%1%%2"_s.arg(qAbs(deltaPercent)).arg(deltaPercent >= 0 ? u'+' : u'-');
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            reportVolume();
        });
        process->start(wpctl, {u"set-volume"_s, u"-l"_s, u"1.0"_s, u"@DEFAULT_AUDIO_SINK@"_s, step});
        return;
    }

    const QString pactl = QStandardPaths::findExecutable(u"pactl"_s);
    if (pactl.isEmpty()) {
        return;
    }
    const QString step = u"%1%2%"_s.arg(deltaPercent >= 0 ? u'+' : u'-').arg(qAbs(deltaPercent));
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        reportVolume();
    });
    process->start(pactl, {u"set-sink-volume"_s, u"@DEFAULT_SINK@"_s, step});
}

void Application::toggleMute()
{
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (!wpctl.isEmpty()) {
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            reportVolume();
        });
        process->start(wpctl, {u"set-mute"_s, u"@DEFAULT_AUDIO_SINK@"_s, u"toggle"_s});
        return;
    }
    const QString pactl = QStandardPaths::findExecutable(u"pactl"_s);
    if (!pactl.isEmpty()) {
        QProcess::startDetached(pactl, {u"set-sink-mute"_s, u"@DEFAULT_SINK@"_s, u"toggle"_s});
    }
}

void Application::reportVolume()
{
    // Plasma only raises its OSD for changes it made itself, so when the island
    // drives the volume it also reports the result.
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (wpctl.isEmpty()) {
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        const QString output = QString::fromUtf8(process->readAllStandardOutput());
        // "Volume: 0.42" or "Volume: 0.42 [MUTED]"
        static const QRegularExpression pattern(u"Volume:\\s*([0-9.]+)"_s);
        const auto match = pattern.match(output);
        if (!match.hasMatch()) {
            return;
        }
        const bool muted = output.contains(u"MUTED"_s);
        const int percent = muted ? 0 : qRound(match.captured(1).toDouble() * 100.0);
        m_osd->showProgress(percent <= 0 ? u"audio-volume-muted"_s
                                         : (percent < 34 ? u"audio-volume-low"_s
                                                         : (percent < 67 ? u"audio-volume-medium"_s
                                                                         : u"audio-volume-high"_s)),
                            percent,
                            muted ? tr("Muted") : tr("Volume"));
    });
    process->start(wpctl, {u"get-volume"_s, u"@DEFAULT_AUDIO_SINK@"_s});
}

void Application::activateApp(const QString &desktopEntry)
{
    if (desktopEntry.isEmpty()) {
        return;
    }
    const QString entry = desktopEntry.endsWith(u".desktop"_s) ? desktopEntry : desktopEntry + u".desktop"_s;

    const QString kstart = QStandardPaths::findExecutable(u"kstart"_s);
    if (!kstart.isEmpty()) {
        QProcess::startDetached(kstart, {entry});
        return;
    }
    const QString gtkLaunch = QStandardPaths::findExecutable(u"gtk-launch"_s);
    if (!gtkLaunch.isEmpty()) {
        QProcess::startDetached(gtkLaunch, {entry});
    }
}

void Application::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void Application::copyText(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

bool Application::islandRunning() const
{
    auto bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(u"org.atoll.Atoll"_s);
}

void Application::startIsland()
{
    const QString binary = QCoreApplication::applicationFilePath();

    // If the island is supervised by the bundled user service, go through
    // systemd: starting it by hand from here would leave it parented to
    // whatever launched the settings window, outside its own unit.
    const QString systemctl = QStandardPaths::findExecutable(u"systemctl"_s);
    if (!systemctl.isEmpty()) {
        QProcess probe;
        probe.start(systemctl, {u"--user"_s, u"is-enabled"_s, u"atoll.service"_s});
        if (probe.waitForFinished(2000) && probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0) {
            QProcess::startDetached(systemctl, {u"--user"_s, u"restart"_s, u"atoll.service"_s});
            return;
        }
    }

    if (!islandRunning()) {
        QProcess::startDetached(binary, {});
        return;
    }

    // Restart: the running island refuses to be started twice, so it has to go
    // away first. The delay is for the bus name to be released.
    auto message = QDBusMessage::createMethodCall(u"org.atoll.Atoll"_s, u"/Atoll"_s, u"org.atoll.Atoll"_s, u"quit"_s);
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
    QTimer::singleShot(600, this, [binary] {
        QProcess::startDetached(binary, {});
    });
}

void Application::openSettings(const QString &page)
{
    // A second process, because layer-shell is a process-wide decision and the
    // settings page wants an ordinary, movable, resizable window. If one is
    // already up it claims the settings bus name and raises itself instead.
    //
    // Asking for layer-shell is done by setting an environment variable, which
    // a child process inherits - so the settings window would come up as a
    // layer surface of its own unless that variable is taken away here.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(u"QT_WAYLAND_SHELL_INTEGRATION"_s);

    auto *process = new QProcess(this);
    process->setProgram(QCoreApplication::applicationFilePath());
    process->setArguments({u"--settings"_s});
    // Which page to land on travels in the environment rather than on the
    // command line, so that raising an already open window and starting a new
    // one do not need two different mechanisms.
    if (!page.isEmpty()) {
        environment.insert(u"ATOLL_SETTINGS_PAGE"_s, page);
    }
    process->setProcessEnvironment(environment);
    process->startDetached();
    process->deleteLater();
}
