/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Atoll - a dynamic island for KDE Plasma.
 */
#include "ai/permissionhook.h"
#include "app/application.h"
#include "app/imagestore.h"
#include "ipc/ipcservice.h"

#include <LayerShellQt/Shell>

#include <KLocalizedString>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace
{
/** Forward a one-shot command to an already running instance. */
bool sendTo(const QString &service,
            const QString &path,
            const QString &method,
            const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(service, path, u"org.atoll.Atoll"_s, method);
    if (!arguments.isEmpty()) {
        message.setArguments(arguments);
    }
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    return reply.type() != QDBusMessage::ErrorMessage;
}

/**
 * The first window the engine produced. In island mode the root object is the
 * instantiator that spreads islands across outputs, not a window, so the
 * search ends at whatever the platform is actually showing.
 */
QQuickWindow *firstWindow(const QQmlApplicationEngine &engine)
{
    for (QObject *root : engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(root)) {
            return window;
        }
        if (auto *window = root->findChild<QQuickWindow *>()) {
            return window;
        }
    }
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
            return quick;
        }
    }
    return nullptr;
}

/**
 * The settings page runs as its own process on an ordinary window, because
 * asking for layer-shell is a process-wide switch and a dialog that cannot be
 * moved, resized or alt-tabbed to is not a settings page. So the flag has to
 * be read before QGuiApplication exists.
 */
bool wantsSettings(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--settings") == 0) {
            return true;
        }
    }
    return false;
}

/**
 * The permission gate runs once per tool call, so it has to start, ask and
 * exit; it never draws anything and must not touch the display at all. Like
 * the settings flag, that has to be known before an application object exists.
 */
bool wantsPermissionHook(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--permission-hook") == 0) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char *argv[])
{
    if (wantsPermissionHook(argc, argv)) {
        QCoreApplication hook(argc, argv);
        return runPermissionHook();
    }

    QGuiApplication::setDesktopSettingsAware(true);
    const bool settingsMode = wantsSettings(argc, argv);

    // Layer-shell surfaces must be requested before the first window is created.
    // ATOLL_NO_LAYER_SHELL falls back to an ordinary top-level window, which is
    // how the island can be inspected under X11, in a nested compositor, or
    // when a compositor's layer-shell support is misbehaving.
    if (!settingsMode && qEnvironmentVariableIntValue("ATOLL_NO_LAYER_SHELL") <= 0) {
        LayerShellQt::Shell::useLayerShell();
    }
    if (settingsMode) {
        // useLayerShell() works by setting this, and it is inherited by any
        // process started from the island. The settings window is a normal
        // window, and a layer surface cannot be moved, resized or focused.
        qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"atoll"_s);
    QGuiApplication::setApplicationDisplayName(u"Atoll"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(ATOLL_VERSION));
    QGuiApplication::setOrganizationName(u"atoll"_s);
    QGuiApplication::setDesktopFileName(u"io.github.atoll.Atoll"_s);
    QGuiApplication::setQuitOnLastWindowClosed(settingsMode);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("atoll"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A dynamic island for KDE Plasma: a morphing overlay that mirrors\n"
                       "Plasma's OSD, your notifications and whatever is playing."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption toggleOption(u"toggle"_s, u"Expand or collapse the running island and exit."_s);
    const QCommandLineOption expandOption(u"expand"_s, u"Expand the running island and exit."_s);
    const QCommandLineOption collapseOption(u"collapse"_s, u"Collapse the running island and exit."_s);
    const QCommandLineOption dismissOption(u"dismiss"_s, u"Dismiss whatever the island is showing."_s);
    const QCommandLineOption quitOption(u"quit"_s, u"Ask the running island to exit."_s);
    const QCommandLineOption settingsOption(u"settings"_s, u"Open the settings window."_s);
    const QCommandLineOption askOption(u"ask"_s,
                                       u"Put a question to the assistant on the running island."_s,
                                       u"question"_s);
    const QCommandLineOption hookOption(
        u"permission-hook"_s,
        u"Ask the running island to judge one tool call. Read on standard input, answered on "
        "standard output; the assistant starts this itself."_s);
    parser.addOption(toggleOption);
    parser.addOption(expandOption);
    parser.addOption(collapseOption);
    parser.addOption(dismissOption);
    parser.addOption(quitOption);
    parser.addOption(settingsOption);
    parser.addOption(askOption);
    parser.addOption(hookOption);
    parser.process(app);

    if (!settingsMode && parser.isSet(askOption)) {
        if (sendTo(u"org.atoll.Atoll"_s, u"/Atoll"_s, u"ask"_s, {parser.value(askOption)})) {
            return 0;
        }
        qWarning("atoll: no running island to ask");
        return 1;
    }

    if (!settingsMode) {
        for (const auto &[option, method] : {std::pair{toggleOption, u"toggle"_s},
                                             std::pair{expandOption, u"expand"_s},
                                             std::pair{collapseOption, u"collapse"_s},
                                             std::pair{dismissOption, u"dismiss"_s},
                                             std::pair{quitOption, u"quit"_s}}) {
            if (parser.isSet(option)) {
                if (sendTo(u"org.atoll.Atoll"_s, u"/Atoll"_s, method)) {
                    return 0;
                }
                qWarning("atoll: no running instance to talk to");
                return 1;
            }
        }
    }

    // Two islands would fight over the same bus name and draw on top of each
    // other; one process already serves every output it was asked for. A
    // restart overlaps though - systemd starts the new instance while the old
    // one is still letting go of the name - so the name is worth waiting for
    // before concluding that somebody else owns it.
    if (!settingsMode && QDBusConnection::sessionBus().isConnected()) {
        const auto taken = [] {
            auto *interface = QDBusConnection::sessionBus().interface();
            return interface && interface->isServiceRegistered(u"org.atoll.Atoll"_s);
        };
        for (int waited = 0; waited < 3000 && taken(); waited += 100) {
            QThread::msleep(100);
        }
        if (taken()) {
            // Exiting with a failure is what lets the user service retry
            // rather than sit there looking like it started and stopped.
            qWarning("atoll: an island is already running");
            return 1;
        }
    }

    QQuickStyle::setStyle(u"Basic"_s);

    Application backend(nullptr);

    // Without a session bus there is nothing to collide with, so a missing bus
    // must not be mistaken for a settings window that is already open.
    if (settingsMode && QDBusConnection::sessionBus().isConnected() && !backend.ipc()->registerSettingsOnBus()) {
        // Settings are already open somewhere; give that window the focus
        // rather than putting a second one next to it.
        sendTo(u"org.atoll.AtollSettings"_s, u"/Settings"_s, u"raise"_s);
        return 0;
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(u"atoll"_s, new ImageStoreProvider);
    engine.addImageProvider(u"icon"_s, new IconProvider);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [settingsMode] {
            qCritical("atoll: %s failed to load", settingsMode ? "the settings window" : "the island");
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("Atoll", settingsMode ? "SettingsWindow" : "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (!settingsMode) {
        backend.start();
    }

    // ATOLL_DEBUG_GRAB=<path> writes one rendered frame to disk and exits;
    // it separates "the island did not draw" from "the compositor did not
    // show it", which are otherwise indistinguishable from the outside.
    const QString grabPath = qEnvironmentVariable("ATOLL_DEBUG_GRAB");
    if (!grabPath.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("ATOLL_DEBUG_GRAB_DELAY");
        QTimer::singleShot(delay > 0 ? delay : 3000, &app, [&engine, grabPath] {
            if (QQuickWindow *window = firstWindow(engine)) {
                const QImage frame = window->grabWindow();
                qWarning("atoll: grabbed %dx%d -> %s (saved: %d)",
                         frame.width(), frame.height(), qUtf8Printable(grabPath),
                         int(frame.save(grabPath)));
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
